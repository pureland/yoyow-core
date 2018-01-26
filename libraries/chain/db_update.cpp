/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/chain/database.hpp>
#include <graphene/chain/db_with.hpp>

#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/transaction_object.hpp>
#include <graphene/chain/withdraw_permission_object.hpp>
#include <graphene/chain/witness_object.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>

#include <fc/uint128.hpp>

namespace graphene { namespace chain {

void database::update_global_dynamic_data( const signed_block& b )
{
   const global_property_object& gpo = get_global_properties();
   const dynamic_global_property_object& _dgp =
      dynamic_global_property_id_type(0)(*this);

   uint32_t missed_blocks = get_slot_at_time( b.timestamp );
   assert( missed_blocks != 0 );
   missed_blocks--;
   for( uint32_t i = 0; i < missed_blocks; ++i ) {
      const auto& witness_missed = get_witness_by_uid( get_scheduled_witness( i+1 ) );
      if(  witness_missed.account != b.witness ) {
         /*
         const auto& witness_account = witness_missed.account(*this);
         if( (fc::time_point::now() - b.timestamp) < fc::seconds(30) )
            wlog( "Witness ${name} missed block ${n} around ${t}", ("name",witness_account.name)("n",b.block_num())("t",b.timestamp) );
            */

         modify( witness_missed, [&]( witness_object& w ) {
           w.total_missed++;
           if( w.last_confirmed_block_num + gpo.parameters.max_witness_inactive_blocks < b.block_num() )
              w.signing_key = public_key_type();
         });
         modify( get_account_statistics_by_uid( witness_missed.account ), [&]( account_statistics_object& s ) {
           s.witness_total_missed++;
         });
      } 
   }

   // dynamic global properties updating
   modify( _dgp, [&]( dynamic_global_property_object& dgp ){
      if( BOOST_UNLIKELY( b.block_num() == 1 ) )
         dgp.recently_missed_count = 0;
         else if( _checkpoints.size() && _checkpoints.rbegin()->first >= b.block_num() )
         dgp.recently_missed_count = 0;
      else if( missed_blocks )
         dgp.recently_missed_count += GRAPHENE_RECENTLY_MISSED_COUNT_INCREMENT*missed_blocks;
      else if( dgp.recently_missed_count > GRAPHENE_RECENTLY_MISSED_COUNT_INCREMENT )
         dgp.recently_missed_count -= GRAPHENE_RECENTLY_MISSED_COUNT_DECREMENT;
      else if( dgp.recently_missed_count > 0 )
         dgp.recently_missed_count--;

      dgp.head_block_number = b.block_num();
      dgp.head_block_id = b.id();
      dgp.time = b.timestamp;
      dgp.current_witness = b.witness;
      dgp.recent_slots_filled = (
           (dgp.recent_slots_filled << 1)
           + 1) << missed_blocks;
      dgp.current_aslot += missed_blocks+1;
   });

   if( !(get_node_properties().skip_flags & skip_undo_history_check) )
   {
      GRAPHENE_ASSERT( _dgp.head_block_number - _dgp.last_irreversible_block_num  < GRAPHENE_MAX_UNDO_HISTORY, undo_database_exception,
                 "The database does not have enough undo history to support a blockchain with so many missed blocks. "
                 "Please add a checkpoint if you would like to continue applying blocks beyond this point.",
                 ("last_irreversible_block_num",_dgp.last_irreversible_block_num)("head", _dgp.head_block_number)
                 ("recently_missed",_dgp.recently_missed_count)("max_undo",GRAPHENE_MAX_UNDO_HISTORY) );
   }
}

void database::update_undo_db_size()
{
   const dynamic_global_property_object& _dgp = dynamic_global_property_id_type(0)(*this);

   _undo_db.set_max_size( _dgp.head_block_number - _dgp.last_irreversible_block_num + 1 );
   _fork_db.set_max_size( _dgp.head_block_number - _dgp.last_irreversible_block_num + 1 );
}

void database::update_signing_witness(const witness_object& signing_witness, const signed_block& new_block)
{
   const global_property_object& gpo = get_global_properties();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   uint64_t new_block_aslot = dpo.current_aslot + get_slot_at_time( new_block.timestamp );

   const auto& itr = gpo.active_witnesses.find( signing_witness.account );
   FC_ASSERT( itr != gpo.active_witnesses.end() );
   auto wit_type = itr->second;

   const auto& core_asset = asset_id_type()(*this);
   share_type budget_this_block = std::min( dpo.total_budget_per_block, core_asset.reserved( *this ) );

   share_type witness_pay;
   if( wit_type == scheduled_by_vote_top )
      witness_pay = gpo.parameters.by_vote_top_witness_pay_per_block;
   else if( wit_type == scheduled_by_vote_rest )
      witness_pay = gpo.parameters.by_vote_rest_witness_pay_per_block;
   else if( wit_type == scheduled_by_pledge )
      witness_pay = gpo.parameters.by_pledge_witness_pay_per_block;
   witness_pay = std::min( witness_pay, budget_this_block );

   share_type budget_remained = budget_this_block - witness_pay;
   FC_ASSERT( budget_remained >= 0 );

   if( budget_this_block > 0 )
   {
      const auto& core_dyn_data = core_asset.dynamic_data(*this);
      modify( core_dyn_data, [&]( asset_dynamic_data_object& dyn )
      {
         dyn.current_supply += budget_this_block;
      } );
   }

   if( budget_remained > 0 )
   {
      modify( dpo, [&]( dynamic_global_property_object& _dpo )
      {
         _dpo.budget_pool += budget_remained;
      } );
   }

   if( witness_pay > 0 )
      deposit_witness_pay( signing_witness, witness_pay );

   modify( signing_witness, [&]( witness_object& _wit )
   {
      _wit.last_aslot = new_block_aslot;
      _wit.total_produced += 1;
      _wit.last_confirmed_block_num = new_block.block_num();
   } );

   modify( get_account_statistics_by_uid( signing_witness.account ), [&]( account_statistics_object& _stat )
   {
      _stat.witness_last_aslot = new_block_aslot;
      _stat.witness_total_produced += 1;
      _stat.witness_last_confirmed_block_num = new_block.block_num();
   } );
}

void database::update_last_irreversible_block()
{
   const global_property_object& gpo = get_global_properties();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   vector< const witness_object* > wit_objs;
   wit_objs.reserve( gpo.active_witnesses.size() );
   for( const auto& wid : gpo.active_witnesses )
      wit_objs.push_back( &(get_witness_by_uid( wid.first )) );

   static_assert( GRAPHENE_IRREVERSIBLE_THRESHOLD > 0, "irreversible threshold must be nonzero" );

   // 1 1 1 2 2 2 2 2 2 2 -> 2     .7*10 = 7
   // 1 1 1 1 1 1 1 2 2 2 -> 1
   // 3 3 3 3 3 3 3 3 3 3 -> 3

   size_t offset = ((GRAPHENE_100_PERCENT - GRAPHENE_IRREVERSIBLE_THRESHOLD) * wit_objs.size() / GRAPHENE_100_PERCENT);

   std::nth_element( wit_objs.begin(), wit_objs.begin() + offset, wit_objs.end(),
      []( const witness_object* a, const witness_object* b )
      {
         return a->last_confirmed_block_num < b->last_confirmed_block_num;
      } );

   uint32_t new_last_irreversible_block_num = wit_objs[offset]->last_confirmed_block_num;

   if( new_last_irreversible_block_num > dpo.last_irreversible_block_num )
   {
      modify( dpo, [&]( dynamic_global_property_object& _dpo )
      {
         _dpo.last_irreversible_block_num = new_last_irreversible_block_num;
      } );
   }
}

void database::clear_expired_transactions()
{ try {
   //Look for expired transactions in the deduplication list, and remove them.
   //Transactions must have expired by at least two forking windows in order to be removed.
   auto& transaction_idx = static_cast<transaction_index&>(get_mutable_index(implementation_ids, impl_transaction_object_type));
   const auto& dedupe_index = transaction_idx.indices().get<by_expiration>();
   while( (!dedupe_index.empty()) && (head_block_time() > dedupe_index.begin()->trx.expiration) )
      transaction_idx.remove(*dedupe_index.begin());
} FC_CAPTURE_AND_RETHROW() }

void database::clear_expired_proposals()
{
   const auto& proposal_expiration_index = get_index_type<proposal_index>().indices().get<by_expiration>();
   while( !proposal_expiration_index.empty() && proposal_expiration_index.begin()->expiration_time <= head_block_time() )
   {
      const proposal_object& proposal = *proposal_expiration_index.begin();
      processed_transaction result;
      try {
         if( proposal.is_authorized_to_execute(*this) )
         {
            result = push_proposal(proposal);
            //TODO: Do something with result so plugins can process it.
            continue;
         }
      } catch( const fc::exception& e ) {
         elog("Failed to apply proposed transaction on its expiration. Deleting it.\n${proposal}\n${error}",
              ("proposal", proposal)("error", e.to_detail_string()));
      }
      remove(proposal);
   }
}

/**
 *  let HB = the highest bid for the collateral  (aka who will pay the most DEBT for the least collateral)
 *  let SP = current median feed's Settlement Price 
 *  let LC = the least collateralized call order's swan price (debt/collateral)
 *
 *  If there is no valid price feed or no bids then there is no black swan.
 *
 *  A black swan occurs if MAX(HB,SP) <= LC
 */
bool database::check_for_blackswan( const asset_object& mia, bool enable_black_swan )
{
    if( !mia.is_market_issued() ) return false;

    const asset_bitasset_data_object& bitasset = mia.bitasset_data(*this);
    if( bitasset.has_settlement() ) return true; // already force settled
    auto settle_price = bitasset.current_feed.settlement_price;
    if( settle_price.is_null() ) return false; // no feed

    const call_order_index& call_index = get_index_type<call_order_index>();
    const auto& call_price_index = call_index.indices().get<by_price>();

    const limit_order_index& limit_index = get_index_type<limit_order_index>();
    const auto& limit_price_index = limit_index.indices().get<by_price>();

    // looking for limit orders selling the most USD for the least CORE
    auto highest_possible_bid = price::max( mia.id, bitasset.options.short_backing_asset );
    // stop when limit orders are selling too little USD for too much CORE
    auto lowest_possible_bid  = price::min( mia.id, bitasset.options.short_backing_asset );

    assert( highest_possible_bid.base.asset_id == lowest_possible_bid.base.asset_id );
    // NOTE limit_price_index is sorted from greatest to least
    auto limit_itr = limit_price_index.lower_bound( highest_possible_bid );
    auto limit_end = limit_price_index.upper_bound( lowest_possible_bid );

    auto call_min = price::min( bitasset.options.short_backing_asset, mia.id );
    auto call_max = price::max( bitasset.options.short_backing_asset, mia.id );
    auto call_itr = call_price_index.lower_bound( call_min );
    auto call_end = call_price_index.upper_bound( call_max );

    if( call_itr == call_end ) return false;  // no call orders

    price highest = settle_price;
    if( limit_itr != limit_end ) {
       assert( settle_price.base.asset_id == limit_itr->sell_price.base.asset_id );
       highest = std::max( limit_itr->sell_price, settle_price );
    }

    auto least_collateral = call_itr->collateralization();
    if( ~least_collateral >= highest  ) 
    {
       elog( "Black Swan detected: \n"
             "   Least collateralized call: ${lc}  ${~lc}\n"
           //  "   Highest Bid:               ${hb}  ${~hb}\n"
             "   Settle Price:              ${sp}  ${~sp}\n"
             "   Max:                       ${h}   ${~h}\n",
            ("lc",least_collateral.to_real())("~lc",(~least_collateral).to_real())
          //  ("hb",limit_itr->sell_price.to_real())("~hb",(~limit_itr->sell_price).to_real())
            ("sp",settle_price.to_real())("~sp",(~settle_price).to_real())
            ("h",highest.to_real())("~h",(~highest).to_real()) );
       FC_ASSERT( enable_black_swan, "Black swan was detected during a margin update which is not allowed to trigger a blackswan" );
       globally_settle_asset(mia, ~least_collateral );
       return true;
    } 
    return false;
}

void database::clear_expired_orders()
{ try {
   detail::with_skip_flags( *this,
      get_node_properties().skip_flags | skip_authority_check, [&](){
         transaction_evaluation_state cancel_context(this);

         //Cancel expired limit orders
         auto& limit_index = get_index_type<limit_order_index>().indices().get<by_expiration>();
         while( !limit_index.empty() && limit_index.begin()->expiration <= head_block_time() )
         {
            limit_order_cancel_operation canceler;
            const limit_order_object& order = *limit_index.begin();
            canceler.fee_paying_account = order.seller;
            canceler.order = order.id;
            canceler.fee = current_fee_schedule().calculate_fee( canceler );
            if( canceler.fee.amount > order.deferred_fee )
            {
               // Cap auto-cancel fees at deferred_fee; see #549
               wlog( "At block ${b}, fee for clearing expired order ${oid} was capped at deferred_fee ${fee}", ("b", head_block_num())("oid", order.id)("fee", order.deferred_fee) );
               canceler.fee = asset( order.deferred_fee, asset_id_type() );
            }
            // we know the fee for this op is set correctly since it is set by the chain.
            // this allows us to avoid a hung chain:
            // - if #549 case above triggers
            // - if the fee is incorrect, which may happen due to #435 (although since cancel is a fixed-fee op, it shouldn't)
            cancel_context.skip_fee_schedule_check = true;
            apply_operation(cancel_context, canceler);
         }
     });

   //Process expired force settlement orders
   auto& settlement_index = get_index_type<force_settlement_index>().indices().get<by_expiration>();
   if( !settlement_index.empty() )
   {
      asset_id_type current_asset = settlement_index.begin()->settlement_asset_id();
      asset max_settlement_volume;
      bool extra_dump = false;

      auto next_asset = [&current_asset, &settlement_index, &extra_dump] {
         auto bound = settlement_index.upper_bound(current_asset);
         if( bound == settlement_index.end() )
         {
            if( extra_dump )
            {
               ilog( "next_asset() returning false" );
            }
            return false;
         }
         if( extra_dump )
         {
            ilog( "next_asset returning true, bound is ${b}", ("b", *bound) );
         }
         current_asset = bound->settlement_asset_id();
         return true;
      };

      uint32_t count = 0;

      // At each iteration, we either consume the current order and remove it, or we move to the next asset
      for( auto itr = settlement_index.lower_bound(current_asset);
           itr != settlement_index.end();
           itr = settlement_index.lower_bound(current_asset) )
      {
         ++count;
         const force_settlement_object& order = *itr;
         auto order_id = order.id;
         current_asset = order.settlement_asset_id();
         const asset_object& mia_object = get(current_asset);
         const asset_bitasset_data_object& mia = mia_object.bitasset_data(*this);

         extra_dump = ((count >= 1000) && (count <= 1020));

         if( extra_dump )
         {
            wlog( "clear_expired_orders() dumping extra data for iteration ${c}", ("c", count) );
            ilog( "head_block_num is ${hb} current_asset is ${a}", ("hb", head_block_num())("a", current_asset) );
         }

         if( mia.has_settlement() )
         {
            ilog( "Canceling a force settlement because of black swan" );
            cancel_order( order );
            continue;
         }

         // Has this order not reached its settlement date?
         if( order.settlement_date > head_block_time() )
         {
            if( next_asset() )
            {
               if( extra_dump )
               {
                  ilog( "next_asset() returned true when order.settlement_date > head_block_time()" );
               }
               continue;
            }
            break;
         }
         // Can we still settle in this asset?
         if( mia.current_feed.settlement_price.is_null() )
         {
            ilog("Canceling a force settlement in ${asset} because settlement price is null",
                 ("asset", mia_object.symbol));
            cancel_order(order);
            continue;
         }
         if( asset_id_type(max_settlement_volume.asset_id) != current_asset )
            max_settlement_volume = mia_object.amount(mia.max_force_settlement_volume(mia_object.dynamic_data(*this).current_supply));
         if( mia.force_settled_volume >= max_settlement_volume.amount )
         {
            /*
            ilog("Skipping force settlement in ${asset}; settled ${settled_volume} / ${max_volume}",
                 ("asset", mia_object.symbol)("settlement_price_null",mia.current_feed.settlement_price.is_null())
                 ("settled_volume", mia.force_settled_volume)("max_volume", max_settlement_volume));
                 */
            if( next_asset() )
            {
               if( extra_dump )
               {
                  ilog( "next_asset() returned true when mia.force_settled_volume >= max_settlement_volume.amount" );
               }
               continue;
            }
            break;
         }

         auto& pays = order.balance;
         auto receives = (order.balance * mia.current_feed.settlement_price);
         receives.amount = (fc::uint128_t(receives.amount.value) *
                            (GRAPHENE_100_PERCENT - mia.options.force_settlement_offset_percent) / GRAPHENE_100_PERCENT).to_uint64();
         assert(receives <= order.balance * mia.current_feed.settlement_price);

         price settlement_price = pays / receives;

         auto& call_index = get_index_type<call_order_index>().indices().get<by_collateral>();
         asset settled = mia_object.amount(mia.force_settled_volume);
         // Match against the least collateralized short until the settlement is finished or we reach max settlements
         while( settled < max_settlement_volume && find_object(order_id) )
         {
            auto itr = call_index.lower_bound(boost::make_tuple(price::min(mia_object.bitasset_data(*this).options.short_backing_asset,
                                                                           mia_object.get_id())));
            // There should always be a call order, since asset exists!
            assert(itr != call_index.end() && itr->debt_type() == mia_object.get_id());
            asset max_settlement = max_settlement_volume - settled;

            if( order.balance.amount == 0 )
            {
               wlog( "0 settlement detected" );
               cancel_order( order );
               break;
            }
            try {
               settled += match(*itr, order, settlement_price, max_settlement);
            } 
            catch ( const black_swan_exception& e ) { 
               wlog( "black swan detected: ${e}", ("e", e.to_detail_string() ) );
               cancel_order( order );
               break;
            }
         }
         if( mia.force_settled_volume != settled.amount )
         {
            modify(mia, [settled](asset_bitasset_data_object& b) {
               b.force_settled_volume = settled.amount;
            });
         }
      }
   }
} FC_CAPTURE_AND_RETHROW() }

void database::update_expired_feeds()
{
   auto& asset_idx = get_index_type<asset_index>().indices().get<by_type>();
   auto itr = asset_idx.lower_bound( true /** market issued */ );
   while( itr != asset_idx.end() )
   {
      const asset_object& a = *itr;
      ++itr;
      assert( a.is_market_issued() );

      const asset_bitasset_data_object& b = a.bitasset_data(*this);
      bool feed_is_expired;
      if( head_block_time() < HARDFORK_615_TIME )
         feed_is_expired = b.feed_is_expired_before_hardfork_615( head_block_time() );
      else
         feed_is_expired = b.feed_is_expired( head_block_time() );
      if( feed_is_expired )
      {
         modify(b, [this](asset_bitasset_data_object& a) {
            a.update_median_feeds(head_block_time());
         });
         check_call_orders(asset_id_type(b.current_feed.settlement_price.base.asset_id)(*this));
      }
      if( !b.current_feed.core_exchange_rate.is_null() &&
          a.options.core_exchange_rate != b.current_feed.core_exchange_rate )
         modify(a, [&b](asset_object& a) {
            a.options.core_exchange_rate = b.current_feed.core_exchange_rate;
         });
   }
}

void database::update_maintenance_flag( bool new_maintenance_flag )
{
   modify( get_dynamic_global_properties(), [&]( dynamic_global_property_object& dpo )
   {
      auto maintenance_flag = dynamic_global_property_object::maintenance_flag;
      dpo.dynamic_flags =
           (dpo.dynamic_flags & ~maintenance_flag)
         | (new_maintenance_flag ? maintenance_flag : 0);
   } );
   return;
}

void database::update_withdraw_permissions()
{
   auto& permit_index = get_index_type<withdraw_permission_index>().indices().get<by_expiration>();
   while( !permit_index.empty() && permit_index.begin()->expiration <= head_block_time() )
      remove(*permit_index.begin());
}

void database::clear_expired_csaf_leases()
{
   const uint64_t csaf_window = get_global_properties().parameters.csaf_accumulate_window;
   const auto head_time = head_block_time();
   const auto& idx = get_index_type<csaf_lease_index>().indices().get<by_expiration>();
   auto itr = idx.begin();
   while( itr != idx.end() && itr->expiration <= head_time )
   {
      modify( get_account_statistics_by_uid( itr->from ), [&](account_statistics_object& s) {
         s.update_coin_seconds_earned( csaf_window, head_time );
         s.core_leased_out -= itr->amount;
      });
      modify( get_account_statistics_by_uid( itr->to ), [&](account_statistics_object& s) {
         s.update_coin_seconds_earned( csaf_window, head_time );
         s.core_leased_in -= itr->amount;
      });
      remove( *itr );
      itr = idx.begin();
   }
}

void database::update_average_witness_pledges()
{
   const auto head_num = head_block_num();
   const auto& idx = get_index_type<witness_index>().indices().get<by_pledge_next_update>();
   auto itr = idx.begin();
   while( itr != idx.end() && itr->average_pledge_next_update_block <= head_num && itr->is_valid )
   {
      update_witness_avg_pledge( *itr );
      itr = idx.begin();
   }
}

void database::release_witness_pledges()
{
   const auto head_num = head_block_num();
   const auto& idx = get_index_type<account_statistics_index>().indices().get<by_witness_pledge_release>();
   auto itr = idx.begin();
   while( itr != idx.end() && itr->witness_pledge_release_block_number <= head_num )
   {
      modify( *itr, [&](account_statistics_object& s) {
         s.total_witness_pledge -= s.releasing_witness_pledge;
         s.releasing_witness_pledge = 0;
         s.witness_pledge_release_block_number = -1;
      });
      itr = idx.begin();
   }
}

void database::release_committee_member_pledges()
{
   const auto head_num = head_block_num();
   const auto& idx = get_index_type<account_statistics_index>().indices().get<by_committee_member_pledge_release>();
   auto itr = idx.begin();
   while( itr != idx.end() && itr->committee_member_pledge_release_block_number <= head_num )
   {
      modify( *itr, [&](account_statistics_object& s) {
         s.total_committee_member_pledge -= s.releasing_committee_member_pledge;
         s.releasing_committee_member_pledge = 0;
         s.committee_member_pledge_release_block_number = -1;
      });
      itr = idx.begin();
   }
}

void database::clear_resigned_witness_votes()
{
   const uint32_t max_votes_to_process = GRAPHENE_MAX_RESIGNED_WITNESS_VOTES_PER_BLOCK;
   uint32_t votes_processed = 0;
   const auto& wit_idx = get_index_type<witness_index>().indices().get<by_valid>();
   const auto& vote_idx = get_index_type<witness_vote_index>().indices().get<by_witness_seq>();
   auto wit_itr = wit_idx.begin(); // assume that false < true
   while( wit_itr != wit_idx.end() && wit_itr->is_valid == false )
   {
      auto vote_itr = vote_idx.lower_bound( std::make_tuple( wit_itr->account, wit_itr->sequence ) );
      while( vote_itr != vote_idx.end()
            && vote_itr->witness_uid == wit_itr->account
            && vote_itr->witness_sequence == wit_itr->sequence )
      {
         const voter_object* voter = find_voter( vote_itr->voter_uid, vote_itr->voter_sequence );
         modify( *voter, [&]( voter_object& v )
         {
            v.number_of_witnesses_voted -= 1;
         } );

         auto tmp_itr = vote_itr;
         ++vote_itr;
         remove( *tmp_itr );

         votes_processed += 1;
         if( votes_processed >= max_votes_to_process )
         {
            ilog( "On block ${n}, reached threshold while removing votes for resigned witnesses", ("n",head_block_num()) );
            return;
         }
      }

      remove( *wit_itr );

      wit_itr = wit_idx.begin();
   }
}

void database::clear_resigned_committee_member_votes()
{
   const uint32_t max_votes_to_process = GRAPHENE_MAX_RESIGNED_COMMITTEE_VOTES_PER_BLOCK;
   uint32_t votes_processed = 0;
   const auto& com_idx = get_index_type<committee_member_index>().indices().get<by_valid>();
   const auto& vote_idx = get_index_type<committee_member_vote_index>().indices().get<by_committee_member_seq>();
   auto com_itr = com_idx.begin(); // assume that false < true
   while( com_itr != com_idx.end() && com_itr->is_valid == false )
   {
      auto vote_itr = vote_idx.lower_bound( std::make_tuple( com_itr->account, com_itr->sequence ) );
      while( vote_itr != vote_idx.end()
            && vote_itr->committee_member_uid == com_itr->account
            && vote_itr->committee_member_sequence == com_itr->sequence )
      {
         const voter_object* voter = find_voter( vote_itr->voter_uid, vote_itr->voter_sequence );
         modify( *voter, [&]( voter_object& v )
         {
            v.number_of_committee_members_voted -= 1;
         } );

         auto tmp_itr = vote_itr;
         ++vote_itr;
         remove( *tmp_itr );

         votes_processed += 1;
         if( votes_processed >= max_votes_to_process )
         {
            ilog( "On block ${n}, reached threshold while removing votes for resigned committee members", ("n",head_block_num()) );
            return;
         }
      }

      remove( *com_itr );

      com_itr = com_idx.begin();
   }
}

void database::update_voter_effective_votes()
{
   const auto head_num = head_block_num();
   const auto& idx = get_index_type<voter_index>().indices().get<by_votes_next_update>();
   auto itr = idx.begin();
   while( itr != idx.end() && itr->effective_votes_next_update_block <= head_num )
   {
      update_voter_effective_votes( *itr );
      itr = idx.begin();
   }
}

void database::invalidate_expired_governance_voters()
{
   const auto expire_blocks = get_global_properties().parameters.governance_voting_expiration_blocks;
   const auto head_num = head_block_num();
   if( head_num < expire_blocks )
      return;
   const auto max_last_vote_block = head_num - expire_blocks;

   uint32_t voters_processed = 0;
   const auto& idx = get_index_type<voter_index>().indices().get<by_valid>();
   auto itr = idx.lower_bound( std::make_tuple( true, GRAPHENE_PROXY_TO_SELF_ACCOUNT_UID ) );
   while( itr != idx.end() && itr->is_valid && itr->proxy_uid == GRAPHENE_PROXY_TO_SELF_ACCOUNT_UID
         && itr->effective_last_vote_block <= max_last_vote_block )
   {
      ++voters_processed;
      const voter_object& voter = *itr;
      ++itr;
      // this voter become invalid.
      invalidate_voter( voter );
   }
   if( voters_processed > 0 )
      ilog( "Invalidated ${n} expired voters", ("n",voters_processed) );
}

void database::process_invalid_governance_voters()
{
   const uint32_t max_voters_to_process = GRAPHENE_MAX_EXPIRED_VOTERS_TO_PROCESS_PER_BLOCK;
   uint32_t voters_processed = 0;
   const auto& idx = get_index_type<voter_index>().indices().get<by_valid>();
   auto itr = idx.begin(); // assume that false < true
   while( voters_processed < max_voters_to_process && itr != idx.end() && itr->is_valid == false )
   {
      // if there is an invalid voter, process the voters who set it as proxy
      voters_processed += process_invalid_proxied_voters( *itr, max_voters_to_process - voters_processed );
      itr = idx.begin(); // this result should be different if still voters_processed < max_voters_to_process
   }
   if( voters_processed >= max_voters_to_process )
      ilog( "On block ${n}, reached threshold while processing invalid voters or proxies", ("n",head_block_num()) );
}

void database::update_committee()
{
   const global_property_object& gpo = get_global_properties();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();

   if( head_block_num() >= dpo.next_committee_update_block )
   {
      // expire all committee proposals
      const auto& idx = get_index_type<committee_proposal_index>().indices();
      for( auto itr = idx.begin(); itr != idx.end(); itr = idx.begin() )
      {
         ilog( "expiring committee proposal #${n}: ${p}", ("n",itr->proposal_number)("p", *itr) );
         remove( *itr );
      }

      // prepare to update active_committee_members
      flat_set<account_uid_type> new_committee;
      new_committee.reserve( gpo.parameters.committee_size );

      // by vote top committee members
      const auto& top_idx = get_index_type<committee_member_index>().indices().get<by_votes>();
      auto top_itr = top_idx.lower_bound( true );
      while( top_itr != top_idx.end() && new_committee.size() < gpo.parameters.committee_size )
      {
         new_committee.insert( top_itr->account );
         ++top_itr;
      }

      // update active_committee_members
      modify( gpo, [&]( global_property_object& gp )
      {
         gp.active_committee_members.swap( new_committee );
      });

      // update dynamic global property object
      modify( dpo, [&]( dynamic_global_property_object& dp )
      {
         dp.next_committee_update_block += gpo.parameters.committee_update_interval;
      });

      ilog( "committee updated on block ${n}, next scheduled update block is ${b}",
            ("n",head_block_num())("b",dpo.next_committee_update_block) );
   }
}

void database::adjust_budgets()
{
   const global_property_object& gpo = get_global_properties();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   if( head_block_num() >= dpo.next_budget_adjust_block )
   {
      const auto& gparams = gpo.parameters;
      share_type core_reserved = asset_id_type()(*this).reserved(*this);
      // Normally shouldn't overflow
      uint32_t blocks_per_year = 86400 * 365 / gparams.block_interval
                               - 86400 * 365 * gparams.maintenance_skip_slots / gparams.maintenance_interval;
      uint64_t new_budget = ( fc::uint128_t( core_reserved.value ) * gparams.budget_adjust_target
                              / blocks_per_year / GRAPHENE_100_PERCENT ).to_uint64();
      modify( dpo, [&]( dynamic_global_property_object& _dpo )
      {
         _dpo.total_budget_per_block = new_budget;
         _dpo.next_budget_adjust_block += gpo.parameters.budget_adjust_interval;
      } );

      ilog( "budgets adjusted on block ${n}, next scheduled adjust block is ${b}",
            ("n",head_block_num())("b",dpo.next_budget_adjust_block) );
   }
}

void database::clear_unapproved_committee_proposals()
{
   const auto head_num = head_block_num();
   const auto& idx = get_index_type<committee_proposal_index>().indices().get<by_approved_closing_block>();
   auto itr = idx.begin(); // assume false < true
   while( itr != idx.end() && itr->is_approved == false && itr->voting_closing_block_num <= head_num )
   {
      ilog( "removing voting closed but still unapproved committee proposal #${n}: ${p}",
            ("n",itr->proposal_number)("p",*itr) );
      remove( *itr );
      itr = idx.begin();
   }
}

void database::execute_committee_proposals()
{
   const auto head_num = head_block_num();
   const auto& idx = get_index_type<committee_proposal_index>().indices().get<by_approved_execution_block>();
   auto itr = idx.lower_bound( true );
   while( itr != idx.end() && itr->is_approved == true && itr->execution_block_num <= head_num )
   {
      ilog( "executing committee proposal #${n}: ${p}", ("n",itr->proposal_number)("p",*itr) );
      auto old_itr = itr;
      ++itr;
      execute_committee_proposal( *old_itr, true ); // the 2nd param is true, which means if it fail, won't throw fc::exception
   }
}

void database::execute_committee_proposal( const committee_proposal_object& proposal, bool silent_fail )
{
   try
   {
      FC_ASSERT( proposal.is_approved, "proposal should have been approved by the committee" );
      FC_ASSERT( head_block_num() >= proposal.execution_block_num, "has not yet reached execution block number" );

      // check registrar takeovers, and prepare for objects to be updated
      flat_map<account_uid_type, const account_object*> accounts;
      flat_map<account_uid_type, bool> account_is_registrar;
      flat_map<account_uid_type, account_uid_type> takeover_map;
      flat_map<account_uid_type, committee_update_account_priviledge_item_type::account_priviledge_update_options > account_items;
      const committee_update_fee_schedule_item_type* fee_item = nullptr;
      const committee_update_global_parameter_item_type* param_item = nullptr;
      for( const auto& item : proposal.items )
      {
         // account update item
         if( item.which() == committee_proposal_item_type::tag< committee_update_account_priviledge_item_type >::value )
         {
            const auto& account_item = item.get< committee_update_account_priviledge_item_type >();
            const auto& pv = account_item.new_priviledges.value;

            bool first_takeover = false;
            account_uid_type first_takeover_registrar = 0;
            if( account_items.find( account_item.account ) == account_items.end() ) // first time process this account
            {
               account_items[ account_item.account ] = account_item.new_priviledges.value;
               if( pv.is_registrar.valid() && *pv.is_registrar == false )
               {
                  first_takeover = true;
                  FC_ASSERT( pv.takeover_registrar.valid(), "Should have takeover registrar account" );
                  first_takeover_registrar = *pv.takeover_registrar;
               }
            }
            else // this account has been already processed at least once
            {
               auto& mv = account_items[ account_item.account ];
               if( pv.can_vote.valid() )
                  mv.can_vote = pv.can_vote;
               if( pv.is_admin.valid() )
                  mv.is_admin = pv.is_admin;
               if( pv.is_registrar.valid() )
               {
                  if( *pv.is_registrar == false && !mv.is_registrar.valid() ) // if it's the first time to be taken-over
                  {
                     first_takeover = true;
                     FC_ASSERT( pv.takeover_registrar.valid(), "Should have takeover registrar account" );
                     first_takeover_registrar = *pv.takeover_registrar;
                  }
                  mv.is_registrar = pv.is_registrar;
               }
            }

            // cache new takeovers
            if( first_takeover )
            {
               const auto& idx = get_index_type<registrar_takeover_index>().indices().get<by_takeover>();
               auto itr = idx.lower_bound( account_item.account );
               while( itr != idx.end() && itr->takeover_registrar == account_item.account )
               {
                  takeover_map[ itr->original_registrar ] = first_takeover_registrar;
                  ++itr;
               }
            }

            if( accounts.find( account_item.account ) == accounts.end() )
            {
               const account_object* account = &get_account_by_uid( account_item.account );
               accounts[ account_item.account ] = account;
               account_is_registrar[ account_item.account ] = account->is_registrar;
            }

            if( pv.is_registrar.valid() )
            {
               account_is_registrar[ account_item.account ] = *pv.is_registrar;
               if( *pv.is_registrar == true )
                  takeover_map.erase( account_item.account );
            }

            if( pv.takeover_registrar.valid() )
            {
               FC_ASSERT( account_is_registrar[ account_item.account ] == false,
                          "Should not take over an active registrar" );

               if( accounts.find( *pv.takeover_registrar ) != accounts.end() )
                  FC_ASSERT( account_is_registrar[ *pv.takeover_registrar ] == true,
                             "Takeover account should be a registrar already" );
               else
               {
                  const account_object* takeover_account = &get_account_by_uid( *pv.takeover_registrar );
                  FC_ASSERT( takeover_account->is_registrar == true,
                             "Takeover account should be a registrar already" );
                  accounts[ takeover_account->uid ] = takeover_account;
                  account_is_registrar[ takeover_account->uid ] = takeover_account->is_registrar;
               }

               // update cache
               for( auto& t : takeover_map )
               {
                  if( t.second == account_item.account )
                     t.second = *pv.takeover_registrar;
               }
               takeover_map[ account_item.account ] = *pv.takeover_registrar;
            }
         }
         // fee update item
         else if( item.which() == committee_proposal_item_type::tag< committee_update_fee_schedule_item_type >::value )
         {
            fee_item = &item.get< committee_update_fee_schedule_item_type >();
         }
         // parameter update item
         else if( item.which() == committee_proposal_item_type::tag< committee_update_global_parameter_item_type >::value )
         {
            param_item = &item.get< committee_update_global_parameter_item_type >();
         }
      }

      // apply changes : new takeover registrars
      for( const auto& takeover_item : takeover_map )
      {
         const registrar_takeover_object* t = find_registrar_takeover_object( takeover_item.first );
         if( t == nullptr )
         {
            create<registrar_takeover_object>( [&](registrar_takeover_object& o){
               o.original_registrar = takeover_item.first;
               o.takeover_registrar = takeover_item.second;
            });
         }
         else
         {
            modify( *t, [&](registrar_takeover_object& o){
               o.takeover_registrar = takeover_item.second;
            });
         }
      }
      // apply changes : account updates
      for( const auto& account_item : account_items )
      {
         const auto& pv = account_item.second;
         if( pv.is_admin.valid() || pv.is_registrar.valid() )
         {
            const account_object* acc = accounts[ account_item.first ];
            modify( *acc, [&](account_object& a){
               if( pv.is_admin.valid() )
                  a.is_admin = *pv.is_admin;
               if( pv.is_registrar.valid() )
                  a.is_registrar = *pv.is_registrar;
               a.last_update_time = head_block_time();
            });
            if( pv.is_registrar.valid() && *pv.is_registrar == true )
            {
               const registrar_takeover_object* t = find_registrar_takeover_object( account_item.first );
               if( t != nullptr )
                  remove( *t );
            }
         }
         if( pv.can_vote.valid() )
         {
            const account_statistics_object& st = get_account_statistics_by_uid( account_item.first );
            if( *pv.can_vote == false && st.is_voter == true )
               invalidate_voter( *find_voter( st.owner, st.last_voter_sequence )  );
            modify( st, [&](account_statistics_object& a){
               a.can_vote = *pv.can_vote;
            });
         }
      }
      // apply changes : fee schedule update
      if( fee_item != nullptr )
      {
         modify( get_global_properties(), [&]( global_property_object& o )
         {
            auto& cp = o.parameters.current_fees->parameters;
            for( const auto& f : (*fee_item)->parameters )
            {
               fee_parameters params; params.set_which(f.which());
               auto itr = cp.find(params);
               if( itr != cp.end() )
                  *itr = f;
               else
                  cp.insert( f );
            }
         });
      }
      // apply changes : global params update
      if( param_item != nullptr )
      {
         const auto& pv = param_item->value;
         modify( get_global_properties(), [&]( global_property_object& _gpo )
         {
            auto& o = _gpo.parameters;
            if( pv.maximum_transaction_size.valid() )
               o.maximum_transaction_size = *pv.maximum_transaction_size;
            if( pv.maximum_block_size.valid() )
               o.maximum_block_size = *pv.maximum_block_size;
            if( pv.maximum_time_until_expiration.valid() )
               o.maximum_time_until_expiration = *pv.maximum_time_until_expiration;
            if( pv.maximum_authority_membership.valid() )
               o.maximum_authority_membership = *pv.maximum_authority_membership;
            if( pv.max_authority_depth.valid() )
               o.max_authority_depth = *pv.max_authority_depth;
            if( pv.csaf_rate.valid() )
               o.csaf_rate = *pv.csaf_rate;
            if( pv.max_csaf_per_account.valid() )
               o.max_csaf_per_account = *pv.max_csaf_per_account;
            if( pv.csaf_accumulate_window.valid() )
               o.csaf_accumulate_window = *pv.csaf_accumulate_window;
            if( pv.min_witness_pledge.valid() )
               o.min_witness_pledge = *pv.min_witness_pledge;
            if( pv.max_witness_pledge_seconds.valid() )
               o.max_witness_pledge_seconds = *pv.max_witness_pledge_seconds;
            if( pv.witness_avg_pledge_update_interval.valid() )
               o.witness_avg_pledge_update_interval = *pv.witness_avg_pledge_update_interval;
            if( pv.witness_pledge_release_delay.valid() )
               o.witness_pledge_release_delay = *pv.witness_pledge_release_delay;
            if( pv.min_governance_voting_balance.valid() )
               o.min_governance_voting_balance = *pv.min_governance_voting_balance;
            if( pv.governance_voting_expiration_blocks.valid() )
               o.governance_voting_expiration_blocks = *pv.governance_voting_expiration_blocks;
            if( pv.governance_votes_update_interval.valid() )
               o.governance_votes_update_interval = *pv.governance_votes_update_interval;
            if( pv.max_governance_votes_seconds.valid() )
               o.max_governance_votes_seconds = *pv.max_governance_votes_seconds;
            if( pv.max_witnesses_voted_per_account.valid() )
               o.max_witnesses_voted_per_account = *pv.max_witnesses_voted_per_account;
            if( pv.max_witness_inactive_blocks.valid() )
               o.max_witness_inactive_blocks = *pv.max_witness_inactive_blocks;
            if( pv.by_vote_top_witness_pay_per_block.valid() )
               o.by_vote_top_witness_pay_per_block = *pv.by_vote_top_witness_pay_per_block;
            if( pv.by_vote_rest_witness_pay_per_block.valid() )
               o.by_vote_rest_witness_pay_per_block = *pv.by_vote_rest_witness_pay_per_block;
            if( pv.by_pledge_witness_pay_per_block.valid() )
               o.by_pledge_witness_pay_per_block = *pv.by_pledge_witness_pay_per_block;
            if( pv.by_vote_top_witness_count.valid() )
               o.by_vote_top_witness_count = *pv.by_vote_top_witness_count;
            if( pv.by_vote_rest_witness_count.valid() )
               o.by_vote_rest_witness_count = *pv.by_vote_rest_witness_count;
            if( pv.by_pledge_witness_count.valid() )
               o.by_pledge_witness_count = *pv.by_pledge_witness_count;
            if( pv.budget_adjust_interval.valid() )
               o.budget_adjust_interval = *pv.budget_adjust_interval;
            if( pv.budget_adjust_target.valid() )
               o.budget_adjust_target = *pv.budget_adjust_target;
            if( pv.min_committee_member_pledge.valid() )
               o.min_committee_member_pledge = *pv.min_committee_member_pledge;
            if( pv.committee_member_pledge_release_delay.valid() )
               o.committee_member_pledge_release_delay = *pv.committee_member_pledge_release_delay;
            if( pv.witness_report_prosecution_period.valid() )
               o.witness_report_prosecution_period = *pv.witness_report_prosecution_period;
            if( pv.witness_report_allow_pre_last_block.valid() )
               o.witness_report_allow_pre_last_block = *pv.witness_report_allow_pre_last_block;
            if( pv.witness_report_pledge_deduction_amount.valid() )
               o.witness_report_pledge_deduction_amount = *pv.witness_report_pledge_deduction_amount;
            
            if( pv.platform_min_pledge.valid() )
               o.platform_min_pledge = *pv.platform_min_pledge;
            if( pv.platform_pledge_release_delay.valid() )
               o.platform_pledge_release_delay = *pv.platform_pledge_release_delay;
            if( pv.platform_max_vote_per_account.valid() )
               o.platform_max_vote_per_account = *pv.platform_max_vote_per_account;
         });
      }

      // remove the executed proposal
      remove( proposal );

   } catch ( const fc::exception& e ) {
      if( silent_fail )
      {
         if( proposal.execution_block_num >= proposal.expiration_block_num
               || proposal.expiration_block_num <= head_block_num() )
         {
            wlog( "exception thrown while executing committee proposal ${p} :\n${e}\nexpired, removing.",
                  ("p",proposal)("e",e.to_detail_string() ) );
            try {
               remove( proposal );
            } catch( ... ) {}
         }
         else
         {
            wlog( "exception thrown while executing committee proposal ${p} :\n${e}\nwill try again on expiration block #${n}.",
                  ("p",proposal)("e",e.to_detail_string() )("n",proposal.expiration_block_num) );
            try {
               modify( proposal, [&]( committee_proposal_object& cpo ){
                  cpo.execution_block_num = cpo.expiration_block_num;
               });
            } catch( ... ) {}
         }
      }
      else
      {
         wlog( "exception thrown while executing committee proposal ${p} :\n${e}", ("p",proposal)("e",e.to_detail_string() ) );
         throw e;
      }
   }
}

void database::check_invariants()
{
   const auto head_num = head_block_num();
   const global_property_object& gpo = get_global_properties();
   const dynamic_global_property_object& dpo = get_dynamic_global_properties();
   const witness_schedule_object& wso = witness_schedule_id_type()(*this);

   FC_ASSERT( dpo.budget_pool >= 0 );
   FC_ASSERT( dpo.next_budget_adjust_block > head_num );
   FC_ASSERT( dpo.next_committee_update_block > head_num );
   FC_ASSERT( wso.next_schedule_block_num > head_num );
   //if( head_block_num() >= 1285 ) { idump( (dpo) ); }

   share_type total_core_balance = 0;
   share_type total_core_non_bal = dpo.budget_pool;
   share_type total_core_leased_in = 0;
   share_type total_core_leased_out = 0;
   share_type total_core_witness_pledge = 0;
   share_type total_core_committee_member_pledge = 0;

   uint64_t total_voting_accounts = 0;
   share_type total_voting_core_balance = 0;

   const auto& acc_stats_idx = get_index_type<account_statistics_index>().indices();
   for( const auto& s: acc_stats_idx )
   {
      //if( head_block_num() >= 1285 ) { idump( (s) ); }
      FC_ASSERT( s.core_balance == get_balance( s.owner, GRAPHENE_CORE_ASSET_AID ).amount );
      FC_ASSERT( s.core_balance >= 0 );
      FC_ASSERT( s.prepaid >= 0 );
      FC_ASSERT( s.csaf >= 0 );
      FC_ASSERT( s.core_leased_in >= 0 );
      FC_ASSERT( s.core_leased_out >= 0 );
      FC_ASSERT( s.total_witness_pledge >= s.releasing_witness_pledge );
      FC_ASSERT( s.releasing_witness_pledge >= 0 );
      FC_ASSERT( s.total_committee_member_pledge >= s.releasing_committee_member_pledge );
      FC_ASSERT( s.releasing_committee_member_pledge >= 0 );
      FC_ASSERT( s.uncollected_witness_pay >= 0 );
      FC_ASSERT( s.witness_pledge_release_block_number > head_num );
      FC_ASSERT( s.committee_member_pledge_release_block_number > head_num );

      total_core_balance += s.core_balance;
      total_core_non_bal += ( s.prepaid + s.uncollected_witness_pay );
      total_core_leased_in += s.core_leased_in;
      total_core_leased_out += s.core_leased_out;
      total_core_witness_pledge += ( s.total_witness_pledge - s.releasing_witness_pledge );
      total_core_committee_member_pledge += ( s.total_committee_member_pledge - s.releasing_committee_member_pledge );
      FC_ASSERT( s.core_balance >= s.core_leased_out + s.total_witness_pledge + s.total_committee_member_pledge );

      if( s.is_voter )
      {
         ++total_voting_accounts;
         total_voting_core_balance += s.core_balance;
      }
   }
   FC_ASSERT( total_core_leased_in == total_core_leased_out );

   share_type current_supply = asset_id_type()(*this).dynamic_data(*this).current_supply;
   //if( head_block_num() >= 1285 )
   //{ idump( (total_core_balance)(total_core_non_bal)(current_supply)( asset_id_type()(*this).dynamic_data(*this) ) ); }
   FC_ASSERT( total_core_balance + total_core_non_bal  == current_supply );

   share_type total_core_leased = 0;
   const auto& csaf_lease_idx = get_index_type<csaf_lease_index>().indices();
   for( const auto& s: csaf_lease_idx )
   {
      FC_ASSERT( s.amount > 0 );
      total_core_leased += s.amount;
   }
   FC_ASSERT( total_core_leased_out == total_core_leased );

   share_type total_core_balance_indexed = 0;
   const auto& acc_bal_idx = get_index_type<account_balance_index>().indices();
   for( const auto& s: acc_bal_idx )
   {
      FC_ASSERT( s.balance >= 0 );
      if( s.asset_type == GRAPHENE_CORE_ASSET_AID)
         total_core_balance_indexed += s.balance;
   }
   FC_ASSERT( total_core_balance == total_core_balance_indexed );

   uint64_t total_voters = 0;
   uint64_t total_witnesses_voted = 0;
   uint64_t total_committee_members_voted = 0;
   uint64_t total_voter_votes = 0;
   fc::uint128_t total_voter_witness_votes;
   fc::uint128_t total_voter_committee_member_votes;
   vector<share_type> total_got_proxied_votes;
   vector<share_type> total_proxied_votes;
   total_got_proxied_votes.resize( gpo.parameters.max_governance_voting_proxy_level );
   total_proxied_votes.resize( gpo.parameters.max_governance_voting_proxy_level );
   const auto& voter_idx = get_index_type<voter_index>().indices();
   for( const auto& s: voter_idx )
   {
      if( s.is_valid )
      {
         FC_ASSERT( s.effective_votes_next_update_block > head_num );
         const auto& stats = get_account_statistics_by_uid( s.uid );
         FC_ASSERT( stats.last_voter_sequence == s.sequence );
         FC_ASSERT( stats.core_balance == s.votes );
         ++total_voters;
         total_voter_votes += s.votes;
         total_witnesses_voted += s.number_of_witnesses_voted;
         total_committee_members_voted += s.number_of_committee_members_voted;
         if( s.proxy_uid == GRAPHENE_PROXY_TO_SELF_ACCOUNT_UID )
         {
            total_voter_witness_votes += fc::uint128_t( s.total_votes() ) * s.number_of_witnesses_voted;
            total_voter_committee_member_votes += fc::uint128_t( s.total_votes() ) * s.number_of_committee_members_voted;
         }
         else
         {
            FC_ASSERT( s.number_of_witnesses_voted == 0 );
            FC_ASSERT( s.number_of_committee_members_voted == 0 );
            total_proxied_votes[0] += s.effective_votes;
            for( size_t i = 1; i < gpo.parameters.max_governance_voting_proxy_level; ++i )
               total_proxied_votes[i] += s.proxied_votes[i-1];
         }
         for( size_t i = 0; i < gpo.parameters.max_governance_voting_proxy_level; ++i )
            total_got_proxied_votes[i] += s.proxied_votes[i];
      }
   }
   FC_ASSERT( total_voting_accounts == total_voters );
   FC_ASSERT( total_voting_core_balance == total_voter_votes );
   for( size_t i = 0; i < gpo.parameters.max_governance_voting_proxy_level; ++i )
   {
      FC_ASSERT( total_proxied_votes[i] == total_got_proxied_votes[i] );
   }

   share_type total_witness_pledges;
   fc::uint128_t total_witness_received_votes;
   const auto& wit_idx = get_index_type<witness_index>().indices();
   for( const auto& s: wit_idx )
   {
      if( s.is_valid )
      {
         FC_ASSERT( s.average_pledge_next_update_block > head_num );
         FC_ASSERT( s.by_pledge_scheduled_time >= wso.current_by_pledge_time );
         FC_ASSERT( s.by_vote_scheduled_time >= wso.current_by_vote_time );
         const auto& stats = get_account_statistics_by_uid( s.account );
         FC_ASSERT( stats.last_witness_sequence == s.sequence );
         FC_ASSERT( stats.total_witness_pledge - stats.releasing_witness_pledge == s.pledge );
         total_witness_pledges += s.pledge;
         total_witness_received_votes += s.total_votes;
      }
   }
   FC_ASSERT( total_witness_pledges == total_core_witness_pledge );
   FC_ASSERT( total_witness_received_votes == total_voter_witness_votes );

   share_type total_committee_member_pledges;
   fc::uint128_t total_committee_member_received_votes;
   const auto& com_idx = get_index_type<committee_member_index>().indices();
   for( const auto& s: com_idx )
   {
      if( s.is_valid )
      {
         const auto& stats = get_account_statistics_by_uid( s.account );
         FC_ASSERT( stats.last_committee_member_sequence == s.sequence );
         FC_ASSERT( stats.total_committee_member_pledge - stats.releasing_committee_member_pledge == s.pledge );
         total_committee_member_pledges += s.pledge;
         total_committee_member_received_votes += s.total_votes;
      }
   }
   FC_ASSERT( total_committee_member_pledges == total_core_committee_member_pledge );
   FC_ASSERT( total_committee_member_received_votes == total_voter_committee_member_votes );


   fc::uint128_t total_witness_vote_objects;
   const auto& wit_vote_idx = get_index_type<witness_vote_index>().indices();
   for( const auto& s: wit_vote_idx )
   {
      const auto wit = find_witness_by_uid( s.witness_uid );
      const auto voter = find_voter( s.voter_uid, s.voter_sequence );
      if( wit != nullptr && voter != nullptr && voter->is_valid )
      {
         ++total_witness_vote_objects;
      }
   }
   FC_ASSERT( total_witnesses_voted == total_witnesses_voted );

   fc::uint128_t total_committee_member_vote_objects;
   const auto& com_vote_idx = get_index_type<committee_member_vote_index>().indices();
   for( const auto& s: com_vote_idx )
   {
      const auto com = find_committee_member_by_uid( s.committee_member_uid );
      const auto voter = find_voter( s.voter_uid, s.voter_sequence );
      if( com != nullptr && voter != nullptr && voter->is_valid )
      {
         ++total_committee_member_vote_objects;
      }
   }
   FC_ASSERT( total_committee_members_voted == total_committee_members_voted );
}

void database::release_platform_pledges()
{
   const auto head_num = head_block_num();
   const auto& idx = get_index_type<account_statistics_index>().indices().get<by_platform_pledge_release>();
   auto itr = idx.begin();
   while( itr != idx.end() && itr->platform_pledge_release_block_number <= head_num )
   {
      modify( *itr, [&](account_statistics_object& s) {
         s.total_platform_pledge -= s.releasing_platform_pledge;
         s.releasing_platform_pledge = 0;
         s.platform_pledge_release_block_number = -1;
      });
      itr = idx.begin();
   }
}

void database::clear_resigned_platform_votes()
{
   const uint32_t max_votes_to_process = GRAPHENE_MAX_RESIGNED_PLATFORM_VOTES_PER_BLOCK;
   uint32_t votes_processed = 0;
   const auto& pla_idx = get_index_type<platform_index>().indices().get<by_valid>();
   const auto& vote_idx = get_index_type<platform_vote_index>().indices().get<by_platform_owner_seq>();
   auto pla_itr = pla_idx.begin(); // assume that false < true
   while( pla_itr != pla_idx.end() && pla_itr->is_valid == false )
   {
      auto vote_itr = vote_idx.lower_bound( std::make_tuple( pla_itr->owner, pla_itr->sequence ) );
      while( vote_itr != vote_idx.end()
            && vote_itr->platform_uid == pla_itr->owner
            && vote_itr->platform_sequence == pla_itr->sequence )
      {
         const voter_object* voter = find_voter( vote_itr->voter_uid, vote_itr->voter_sequence );
         modify( *voter, [&]( voter_object& v )
         {
            v.number_of_platform_voted -= 1;
         } );

         auto tmp_itr = vote_itr;
         ++vote_itr;
         remove( *tmp_itr );

         votes_processed += 1;
         if( votes_processed >= max_votes_to_process )
         {
            ilog( "On block ${n}, reached threshold while removing votes for resigned platforms", ("n",head_block_num()) );
            return;
         }
      }

      remove( *pla_itr );

      pla_itr = pla_idx.begin();
   }
}

} }
