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

#include <graphene/chain/protocol/authority.hpp>
#include <graphene/app/impacted.hpp>

namespace graphene { namespace app {

using namespace fc;
using namespace graphene::chain;

// TODO:  Review all of these, especially no-ops
struct get_impacted_account_uid_visitor
{
   flat_set<account_uid_type>& _impacted;
   get_impacted_account_uid_visitor( flat_set<account_uid_type>& impact ):_impacted(impact) {}
   typedef void result_type;

   // fee payer will be checked outside
   template<typename T>
   void operator()( const T& op ) {}

   void operator()( const account_create_operation& op )
   {
      _impacted.insert( op.uid );
      //_impacted.insert( op.reg_info.registrar ); // fee payer
      _impacted.insert( op.reg_info.referrer );
      add_authority_account_uids( _impacted, op.owner );
      add_authority_account_uids( _impacted, op.active );
      add_authority_account_uids( _impacted, op.secondary );
   }

   void operator()( const transfer_operation& op )
   {
      //_impacted.insert( op.from ); // fee payer
      _impacted.insert( op.to );
   }

   void operator()( const post_operation& op )
   {
      //_impacted.insert( op.poster ); // fee payer
      if( op.parent_poster.valid() )
         _impacted.insert( *(op.parent_poster) );
   }

   void operator()( const account_manage_operation& op )
   {
      //_impacted.insert( op.executor ); // fee payer
      _impacted.insert( op.account );
   }

   void operator()( const csaf_collect_operation& op )
   {
      //_impacted.insert( op.from ); // fee payer
      _impacted.insert( op.to );
   }

   void operator()( const csaf_lease_operation& op )
   {
      //_impacted.insert( op.from ); // fee payer
      _impacted.insert( op.to );
   }

   void operator()( const account_update_key_operation& op )
   {
      //_impacted.insert( op.fee_payer ); // fee payer
      _impacted.insert( op.uid );
   }

   void operator()( const account_update_auth_operation& op )
   {
      //_impacted.insert( op.uid ); // fee payer
      if( op.owner.valid() )
         add_authority_account_uids( _impacted, *op.owner );
      if( op.active.valid() )
         add_authority_account_uids( _impacted, *op.active );
      if( op.secondary.valid() )
         add_authority_account_uids( _impacted, *op.secondary );
   }

   void operator()( const account_update_proxy_operation& op )
   {
      //_impacted.insert( op.voter ); // fee payer
      _impacted.insert( op.proxy );
   }

   void operator()( const witness_create_operation& op )
   {
      //_impacted.insert( op.account ); // fee payer
   }

   void operator()( const witness_update_operation& op )
   {
      //_impacted.insert( op.account ); // fee payer
   }

   void operator()( const witness_vote_update_operation& op )
   {
      //_impacted.insert( op.voter ); // fee payer
      for( auto uid : op.witnesses_to_add )
         _impacted.insert( uid );
      for( auto uid : op.witnesses_to_remove )
         _impacted.insert( uid );
   }

   void operator()( const witness_collect_pay_operation& op )
   {
      //_impacted.insert( op.account ); // fee payer
   }

   void operator()( const witness_report_operation& op )
   {
      //_impacted.insert( op.reporter ); // fee payer
      _impacted.insert( op.first_block.witness );
   }

   void operator()( const platform_create_operation& op )
   {
      //_impacted.insert( op.owner ); // fee payer
   }

   void operator()( const platform_update_operation& op )
   {
      //_impacted.insert( op.owner ); // fee payer
   }

   void operator()( const platform_vote_update_operation& op )
   {
      //_impacted.insert( op.voter ); // fee payer
      for( auto uid : op.platform_to_add )
         _impacted.insert( uid );
      for( auto uid : op.platform_to_remove )
         _impacted.insert( uid );
   }

   void operator()( const committee_member_create_operation& op )
   {
      //_impacted.insert( op.account ); // fee payer
   }
   void operator()( const committee_member_update_operation& op )
   {
      //_impacted.insert( op.account ); // fee payer
   }
   void operator()( const committee_member_vote_update_operation& op )
   {
      //_impacted.insert( op.voter ); // fee payer
      for( auto uid : op.committee_members_to_add )
         _impacted.insert( uid );
      for( auto uid : op.committee_members_to_remove )
         _impacted.insert( uid );
   }
   void operator()( const committee_proposal_create_operation& op )
   {
      //_impacted.insert( op.proposer ); // fee payer
      for( const auto& item : op.items )
      {
         if( item.which() == committee_proposal_item_type::tag< committee_update_account_priviledge_item_type >::value )
         {
            const auto& account_item = item.get< committee_update_account_priviledge_item_type >();
            _impacted.insert( account_item.account );
            if( account_item.new_priviledges.value.takeover_registrar.valid() )
               _impacted.insert( *account_item.new_priviledges.value.takeover_registrar );
         }
      }
   }
   void operator()( const committee_proposal_update_operation& op )
   {
      //_impacted.insert( op.account ); // fee payer
   }
   /*
   void operator()( const balance_claim_operation& op ) {}

   void operator()( const asset_claim_fees_operation& op ){}
   void operator()( const limit_order_create_operation& op ) {}
   void operator()( const limit_order_cancel_operation& op )
   {
      _impacted.insert( op.fee_paying_account );
   }
   void operator()( const call_order_update_operation& op ) {}
   void operator()( const fill_order_operation& op )
   {
      _impacted.insert( op.account_id );
   }

   void operator()( const account_update_operation& op )
   {
      _impacted.insert( op.account );
      if( op.owner )
         add_authority_accounts( _impacted, *(op.owner) );
      if( op.active )
         add_authority_accounts( _impacted, *(op.active) );
   }

   void operator()( const account_whitelist_operation& op )
   {
      _impacted.insert( op.account_to_list );
   }

   void operator()( const account_upgrade_operation& op ) {}
   void operator()( const account_transfer_operation& op )
   {
      _impacted.insert( op.new_owner );
   }

   void operator()( const asset_create_operation& op ) {}
   void operator()( const asset_update_operation& op )
   {
      if( op.new_issuer )
         _impacted.insert( *(op.new_issuer) );
   }

   void operator()( const asset_update_bitasset_operation& op ) {}
   void operator()( const asset_update_feed_producers_operation& op ) {}

   void operator()( const asset_issue_operation& op )
   {
      _impacted.insert( op.issue_to_account );
   }

   void operator()( const asset_reserve_operation& op ) {}
   void operator()( const asset_fund_fee_pool_operation& op ) {}
   void operator()( const asset_settle_operation& op ) {}
   void operator()( const asset_global_settle_operation& op ) {}
   void operator()( const asset_publish_feed_operation& op ) {}
   void operator()( const proposal_create_operation& op )
   {
      vector<authority> other;
      for( const auto& proposed_op : op.proposed_ops )
         operation_get_required_authorities( proposed_op.op, _impacted, _impacted, other );
      for( auto& o : other )
         add_authority_accounts( _impacted, o );
   }

   void operator()( const proposal_update_operation& op ) {}
   void operator()( const proposal_delete_operation& op ) {}

   void operator()( const withdraw_permission_create_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const withdraw_permission_update_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const withdraw_permission_claim_operation& op )
   {
      _impacted.insert( op.withdraw_from_account );
   }

   void operator()( const withdraw_permission_delete_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const committee_member_update_global_parameters_operation& op ) {}

   void operator()( const vesting_balance_create_operation& op )
   {
      _impacted.insert( op.owner );
   }

   void operator()( const vesting_balance_withdraw_operation& op ) {}
   void operator()( const worker_create_operation& op ) {}
   void operator()( const custom_operation& op ) {}
   void operator()( const assert_operation& op ) {}

   void operator()( const override_transfer_operation& op )
   {
      _impacted.insert( op.to );
      _impacted.insert( op.from );
      _impacted.insert( op.issuer );
   }

   void operator()( const transfer_to_blind_operation& op )
   {
      _impacted.insert( op.from );
      for( const auto& out : op.outputs )
         add_authority_accounts( _impacted, out.owner );
   }

   void operator()( const blind_transfer_operation& op )
   {
      for( const auto& in : op.inputs )
         add_authority_accounts( _impacted, in.owner );
      for( const auto& out : op.outputs )
         add_authority_accounts( _impacted, out.owner );
   }

   void operator()( const transfer_from_blind_operation& op )
   {
      _impacted.insert( op.to );
      for( const auto& in : op.inputs )
         add_authority_accounts( _impacted, in.owner );
   }

   void operator()( const asset_settle_cancel_operation& op )
   {
      _impacted.insert( op.account );
   }

   void operator()( const fba_distribute_operation& op )
   {
      _impacted.insert( op.account_id );
   }
   */

};

// TODO:  Review all of these, especially no-ops
struct get_impacted_account_visitor
{
   flat_set<account_id_type>& _impacted;
   get_impacted_account_visitor( flat_set<account_id_type>& impact ):_impacted(impact) {}
   typedef void result_type;

   void operator()( const transfer_operation& op )
   {
      // TODO review
      //_impacted.insert( op.to );
   }

   void operator()( const post_operation& op )
   {
      // TODO review
   }

   void operator()( const csaf_collect_operation& op )
   {
      // TODO review
   }

   void operator()( const csaf_lease_operation& op )
   {
      // TODO review
   }

   void operator()( const account_update_key_operation& op )
   {
      // TODO review
   }

   void operator()( const account_update_auth_operation& op )
   {
      // TODO review
   }

   void operator()( const account_update_proxy_operation& op )
   {
      // TODO review
   }

   void operator()( const asset_claim_fees_operation& op ){}
   void operator()( const limit_order_create_operation& op ) {}
   void operator()( const limit_order_cancel_operation& op )
   {
      _impacted.insert( op.fee_paying_account );
   }
   void operator()( const call_order_update_operation& op ) {}
   void operator()( const fill_order_operation& op )
   {
      _impacted.insert( op.account_id );
   }

   void operator()( const account_create_operation& op )
   {
      //TODO review
      //_impacted.insert( op.registrar );
      //_impacted.insert( op.referrer );
      add_authority_accounts( _impacted, op.owner );
      add_authority_accounts( _impacted, op.active );
   }

   void operator()( const account_manage_operation& op )
   {
   }

   void operator()( const account_update_operation& op )
   {
      _impacted.insert( op.account );
      if( op.owner )
         add_authority_accounts( _impacted, *(op.owner) );
      if( op.active )
         add_authority_accounts( _impacted, *(op.active) );
   }

   void operator()( const account_whitelist_operation& op )
   {
      _impacted.insert( op.account_to_list );
   }

   void operator()( const account_upgrade_operation& op ) {}
   void operator()( const account_transfer_operation& op )
   {
      _impacted.insert( op.new_owner );
   }

   void operator()( const asset_create_operation& op ) {}
   void operator()( const asset_update_operation& op )
   {
      if( op.new_issuer )
         _impacted.insert( *(op.new_issuer) );
   }

   void operator()( const asset_update_bitasset_operation& op ) {}
   void operator()( const asset_update_feed_producers_operation& op ) {}

   void operator()( const asset_issue_operation& op )
   {
      _impacted.insert( op.issue_to_account );
   }

   void operator()( const asset_reserve_operation& op ) {}
   void operator()( const asset_fund_fee_pool_operation& op ) {}
   void operator()( const asset_settle_operation& op ) {}
   void operator()( const asset_global_settle_operation& op ) {}
   void operator()( const asset_publish_feed_operation& op ) {}
   void operator()( const witness_create_operation& op )
   {
      // TODO review
      //_impacted.insert( op.account );
   }
   void operator()( const witness_update_operation& op )
   {
      // TODO review
      //_impacted.insert( op.account );
   }
   void operator()( const witness_vote_update_operation& op )
   {
      // TODO review
   }
   void operator()( const witness_collect_pay_operation& op )
   {
      // TODO review
   }
   void operator()( const witness_report_operation& op )
   {
      // TODO review
   }

   void operator()( const platform_create_operation& op )
   {
      // TODO review
      //_impacted.insert( op.owner );
   }
   void operator()( const platform_update_operation& op )
   {
      // TODO review
      //_impacted.insert( op.owner );
   }
   void operator()( const platform_vote_update_operation& op )
   {
      // TODO review
   }


   void operator()( const proposal_create_operation& op )
   {
      vector<authority> other;
      for( const auto& proposed_op : op.proposed_ops )
         operation_get_required_authorities( proposed_op.op, _impacted, _impacted, other );
      for( auto& o : other )
         add_authority_accounts( _impacted, o );
   }

   void operator()( const proposal_update_operation& op ) {}
   void operator()( const proposal_delete_operation& op ) {}

   void operator()( const withdraw_permission_create_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const withdraw_permission_update_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const withdraw_permission_claim_operation& op )
   {
      _impacted.insert( op.withdraw_from_account );
   }

   void operator()( const withdraw_permission_delete_operation& op )
   {
      _impacted.insert( op.authorized_account );
   }

   void operator()( const committee_member_create_operation& op )
   {
      // TODO review
      //_impacted.insert( op.committee_member_account );
   }
   void operator()( const committee_member_update_operation& op )
   {
      // TODO review
      //_impacted.insert( op.committee_member_account );
   }
   void operator()( const committee_member_vote_update_operation& op )
   {
      // TODO review
   }
   void operator()( const committee_proposal_create_operation& op )
   {
      // TODO review
   }
   void operator()( const committee_proposal_update_operation& op )
   {
      // TODO review
   }
   void operator()( const committee_member_update_global_parameters_operation& op ) {}

   void operator()( const vesting_balance_create_operation& op )
   {
      _impacted.insert( op.owner );
   }

   void operator()( const vesting_balance_withdraw_operation& op ) {}
   void operator()( const worker_create_operation& op ) {}
   void operator()( const custom_operation& op ) {}
   void operator()( const assert_operation& op ) {}
   void operator()( const balance_claim_operation& op ) {}

   void operator()( const override_transfer_operation& op )
   {
      _impacted.insert( op.to );
      _impacted.insert( op.from );
      _impacted.insert( op.issuer );
   }

   void operator()( const transfer_to_blind_operation& op )
   {
      _impacted.insert( op.from );
      for( const auto& out : op.outputs )
         add_authority_accounts( _impacted, out.owner );
   }

   void operator()( const blind_transfer_operation& op )
   {
      for( const auto& in : op.inputs )
         add_authority_accounts( _impacted, in.owner );
      for( const auto& out : op.outputs )
         add_authority_accounts( _impacted, out.owner );
   }

   void operator()( const transfer_from_blind_operation& op )
   {
      _impacted.insert( op.to );
      for( const auto& in : op.inputs )
         add_authority_accounts( _impacted, in.owner );
   }

   void operator()( const asset_settle_cancel_operation& op )
   {
      _impacted.insert( op.account );
   }

   void operator()( const fba_distribute_operation& op )
   {
      _impacted.insert( op.account_id );
   }

};

void operation_get_impacted_account_uids( const operation& op, flat_set<account_uid_type>& result )
{
   get_impacted_account_uid_visitor vtor = get_impacted_account_uid_visitor( result );
   op.visit( vtor );
}

void operation_get_impacted_accounts( const operation& op, flat_set<account_id_type>& result )
{
   get_impacted_account_visitor vtor = get_impacted_account_visitor( result );
   op.visit( vtor );
}

void transaction_get_impacted_accounts( const transaction& tx, flat_set<account_id_type>& result )
{
   for( const auto& op : tx.operations )
      operation_get_impacted_accounts( op, result );
}

void transaction_get_impacted_account_uids( const transaction& tx, flat_set<account_uid_type>& result )
{
   for( const auto& op : tx.operations )
      operation_get_impacted_account_uids( op, result );
}

} }
