
#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/protocol/content.hpp>
#include <graphene/chain/content_object.hpp>
#include <graphene/chain/pledge_mining_object.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/advertising_object.hpp>
#include <graphene/chain/custom_vote_object.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/log/logger.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( operation_tests, database_fixture )

BOOST_AUTO_TEST_CASE(witness_csaf_test)
{
   try
   {
      ACTORS((1000)(2000));
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      transfer(committee_account, u_1000_id, _core(100000));
      transfer(committee_account, u_2000_id, _core(100000));
      const witness_object& witness1 = create_witness(u_1000_id, u_1000_private_key, _core(10000));
      const witness_object& witness2 = create_witness(u_2000_id, u_2000_private_key, _core(10000));


      //###############################  before reduce witness csaf on hardfork_4_time

      collect_csaf({ u_1000_private_key }, u_1000_id, u_1000_id, 1000);
      const account_statistics_object& ants_1000 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(ants_1000.csaf == 1000 * prec);

      //###############################  reduce witness csaf on hardfork_4_time. need to modify hardfork_4_time and debug

      //collect_csaf({ u_1000_private_key }, u_1000_id, u_1000_id, 1000);
      //const account_statistics_object& ants_1000 = db.get_account_statistics_by_uid(u_1000_id);
      //BOOST_CHECK(ants_1000.csaf == 1000 * prec);

   }
   catch (const fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(collect_csaf_test)
{
   try
   {
      ACTORS((1000)(2000));
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      collect_csaf({ u_1000_private_key }, u_1000_id, u_1000_id, 1000);

      const account_statistics_object& ants_1000 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(ants_1000.csaf == 1000 * prec);

      //###############################################################
      collect_csaf_from_committee(u_2000_id, 1000);
      const account_statistics_object& ants_2000 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(ants_2000.csaf == 1000 * prec);
   }
   catch (const fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(committee_proposal_test)
{
   try
   {
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      // Return number of core shares (times precision)
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      // make sure the database requires our fee to be nonzero
      enable_fees();

      for (int i = 0; i < 5; ++i)
         add_csaf_for_account(genesis_state.initial_accounts.at(i).uid, 1000);

      generate_blocks(10);

      committee_update_global_content_parameter_item_type item;
      item.value = { 300, 300, 1000, 31536000, 10, 10000000000000, 10000000000000, 10000000000000, 1, 100,
         3000, 30000, 62000000, 4000, 2000, 8000, 9000, 11000, 20, 2000000, 3640000 };
      committee_proposal_create(genesis_state.initial_accounts.at(0).uid, { item }, 100, voting_opinion_type::opinion_for, 100, 100);
      for (int i = 1; i < 5; ++i)
         committee_proposal_vote(genesis_state.initial_accounts.at(i).uid, 1, voting_opinion_type::opinion_for);

      generate_blocks(101);
      auto gap = db.get_global_properties().parameters.get_award_params();

      BOOST_REQUIRE_EQUAL(gap.content_award_interval, 300);
      BOOST_REQUIRE_EQUAL(gap.platform_award_interval, 300);
      BOOST_REQUIRE_EQUAL(gap.max_csaf_per_approval.value, 1000);
      BOOST_REQUIRE_EQUAL(gap.approval_expiration, 31536000);
      BOOST_REQUIRE_EQUAL(gap.min_effective_csaf.value, 10);
      BOOST_REQUIRE_EQUAL(gap.total_content_award_amount.value, 10000000000000);
      BOOST_REQUIRE_EQUAL(gap.total_platform_content_award_amount.value, 10000000000000);
      BOOST_REQUIRE_EQUAL(gap.total_platform_voted_award_amount.value, 10000000000000);
      BOOST_REQUIRE_EQUAL(gap.platform_award_min_votes.value, 1);
      BOOST_REQUIRE_EQUAL(gap.platform_award_requested_rank, 100);

      BOOST_REQUIRE_EQUAL(gap.platform_award_basic_rate, 3000);
      BOOST_REQUIRE_EQUAL(gap.casf_modulus, 30000);
      BOOST_REQUIRE_EQUAL(gap.post_award_expiration, 62000000);
      BOOST_REQUIRE_EQUAL(gap.approval_casf_min_weight, 4000);
      BOOST_REQUIRE_EQUAL(gap.approval_casf_first_rate, 2000);
      BOOST_REQUIRE_EQUAL(gap.approval_casf_second_rate, 8000);
      BOOST_REQUIRE_EQUAL(gap.receiptor_award_modulus, 9000);
      BOOST_REQUIRE_EQUAL(gap.disapprove_award_modulus, 11000);

      BOOST_REQUIRE_EQUAL(gap.advertising_confirmed_fee_rate, 20);
      BOOST_REQUIRE_EQUAL(gap.advertising_confirmed_min_fee.value, 2000000);
      BOOST_REQUIRE_EQUAL(gap.custom_vote_effective_time, 3640000);

   }
   catch (const fc::exception& e)
   {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(update_post_test)
{
   try{
      ACTORS((1000)(1001)
         (9000));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      // Return number of core shares (times precision)
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      transfer(committee_account, u_1001_id, _core(100000));
      transfer(committee_account, u_9000_id, _core(100000));

      add_csaf_for_account(u_1001_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);

      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", { u_9000_private_key });
      account_auth_platform({ u_1001_private_key }, u_1001_id, u_9000_id, 1000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", { u_9000_private_key });

      post_operation::ext extensions;
      extensions.license_lid = 1;
      create_post({ u_1001_private_key, u_9000_private_key }, u_9000_id, u_1001_id, "", "", "", "",
         optional<account_uid_type>(),
         optional<account_uid_type>(),
         optional<post_pid_type>(),
         extensions);

      post_update_operation::ext ext;
      ext.forward_price = 100 * prec;
      ext.receiptor = u_1001_id;
      ext.to_buyout = true;
      ext.buyout_ratio = 3000;
      ext.buyout_price = 10000 * prec;
      ext.license_lid = 1;
      ext.permission_flags = 0xF;
      update_post({ u_1001_private_key, u_9000_private_key }, u_9000_id, u_1001_id, 1, "", "", "", "", ext);

      auto post_obj = db.get_post_by_platform(u_9000_id, u_1001_id, 1);
      Receiptor_Parameter parameter = post_obj.receiptors[u_1001_id];

      BOOST_CHECK(*(post_obj.forward_price) == 100 * prec);
      BOOST_CHECK(parameter.to_buyout == true);
      BOOST_CHECK(parameter.buyout_ratio == 3000);
      BOOST_CHECK(parameter.buyout_price == 10000 * prec);
      BOOST_CHECK(post_obj.license_lid == 1);
      BOOST_CHECK(post_obj.permission_flags == 0xF);
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(score_test)
{
   try{
      ACTORS((1001)(9000));
      flat_map<account_uid_type, fc::ecc::private_key> score_map;
      actor(1003, 10, score_map);
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      // Return number of core shares (times precision)
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      for (int i = 0; i < 5; ++i)
         add_csaf_for_account(genesis_state.initial_accounts.at(i).uid, 1000);
      transfer(committee_account, u_9000_id, _core(100000));
      generate_blocks(10);

      BOOST_TEST_MESSAGE("Turn on the reward mechanism, open content award and platform voted award");
      committee_update_global_content_parameter_item_type item;
      item.value = { 300, 300, 1000, 31536000, 10, 10000000000000, 10000000000000, 10000000000000, 1000, 100 };
      committee_proposal_create(genesis_state.initial_accounts.at(0).uid, { item }, 100, voting_opinion_type::opinion_for, 100, 100);
      for (int i = 1; i < 5; ++i)
         committee_proposal_vote(genesis_state.initial_accounts.at(i).uid, 1, voting_opinion_type::opinion_for);
      generate_blocks(89);

      for (auto a : score_map)
         add_csaf_for_account(a.first, 10000);
      add_csaf_for_account(u_9000_id, 10000);

      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", { u_9000_private_key });
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", { u_9000_private_key });
      account_auth_platform({ u_1001_private_key }, u_1001_id, u_9000_id, 10000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);

      post_operation::ext extensions;
      extensions.license_lid = 1;
      create_post({ u_1001_private_key, u_9000_private_key }, u_9000_id, u_1001_id, "", "", "", "",
         optional<account_uid_type>(),
         optional<account_uid_type>(),
         optional<post_pid_type>(),
         extensions);

      for (auto a : score_map)
      {
         account_auth_platform({ a.second }, a.first, u_9000_id, 1000 * prec, 0x1F);
         account_manage(a.first, { true, true, true });
         score_a_post({ a.second }, a.first, u_9000_id, u_1001_id, 1, 5, 10);
      }

      const auto& apt_idx = db.get_index_type<active_post_index>().indices().get<by_post_pid>();
      auto apt_itr = apt_idx.find(std::make_tuple(u_9000_id, u_1001_id, 1, 1));
      BOOST_CHECK(apt_itr != apt_idx.end());
      auto active_post = *apt_itr;
      BOOST_CHECK(active_post.total_csaf == 10 * 10);

      for (auto a : score_map)
      {
         auto score_obj = db.get_score(u_9000_id, u_1001_id, 1, a.first);
         BOOST_CHECK(score_obj.score == 5);
         BOOST_CHECK(score_obj.csaf == 10);
      }

      //test clear expired score
      generate_blocks(10);
      generate_block(~0, generate_private_key("null_key"), 10512000);
      for (auto a : score_map)
      {
         auto score_obj = db.find_score(u_9000_id, u_1001_id, 1, a.first);
         BOOST_CHECK(score_obj == nullptr);
      }
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(reward_test)
{
   try{
      ACTORS((1001)(9000));

      flat_map<account_uid_type, fc::ecc::private_key> reward_map;
      actor(1003, 10, reward_map);
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      // Return number of core shares (times precision)
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      transfer(committee_account, u_9000_id, _core(100000));
      generate_blocks(10);

      BOOST_TEST_MESSAGE("Turn on the reward mechanism, open content award and platform voted award");
      committee_update_global_content_parameter_item_type item;
      item.value = { 300, 300, 1000, 31536000, 10, 10000000000000, 10000000000000, 10000000000000, 1000, 100 };
      committee_proposal_create(genesis_state.initial_accounts.at(0).uid, { item }, 100, voting_opinion_type::opinion_for, 100, 100);
      for (int i = 1; i < 5; ++i)
         committee_proposal_vote(genesis_state.initial_accounts.at(i).uid, 1, voting_opinion_type::opinion_for);
      generate_blocks(89);

      for (auto a : reward_map)
         add_csaf_for_account(a.first, 10000);
      add_csaf_for_account(u_9000_id, 10000);

      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", { u_9000_private_key });
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", { u_9000_private_key });
      account_auth_platform({ u_1001_private_key }, u_1001_id, u_9000_id, 10000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);

      post_operation::ext extensions;
      extensions.license_lid = 1;

      create_post({ u_1001_private_key, u_9000_private_key }, u_9000_id, u_1001_id, "", "", "", "",
         optional<account_uid_type>(),
         optional<account_uid_type>(),
         optional<post_pid_type>(),
         extensions);

      for (auto a : reward_map)
      {
         transfer(committee_account, a.first, _core(100000));
         reward_post(a.first, u_9000_id, u_1001_id, 1, _core(1000), { a.second });
      }

      const auto& apt_idx = db.get_index_type<active_post_index>().indices().get<by_post_pid>();
      auto apt_itr = apt_idx.find(std::make_tuple(u_9000_id, u_1001_id, 1, 1));
      BOOST_CHECK(apt_itr != apt_idx.end());
      auto active_post = *apt_itr;
      BOOST_CHECK(active_post.total_rewards.find(GRAPHENE_CORE_ASSET_AID) != active_post.total_rewards.end());
      BOOST_CHECK(active_post.total_rewards[GRAPHENE_CORE_ASSET_AID] == 10 * 1000 * prec);

      BOOST_CHECK(active_post.receiptor_details.find(u_9000_id) != active_post.receiptor_details.end());
      auto iter_reward = active_post.receiptor_details[u_9000_id].rewards.find(GRAPHENE_CORE_ASSET_AID);
      BOOST_CHECK(iter_reward != active_post.receiptor_details[u_9000_id].rewards.end());
      BOOST_CHECK(iter_reward->second == 10 * 250 * prec);

      BOOST_CHECK(active_post.receiptor_details.find(u_1001_id) != active_post.receiptor_details.end());
      auto iter_reward2 = active_post.receiptor_details[u_1001_id].rewards.find(GRAPHENE_CORE_ASSET_AID);
      BOOST_CHECK(iter_reward2 != active_post.receiptor_details[u_1001_id].rewards.end());
      BOOST_CHECK(iter_reward2->second == 10 * 750 * prec);

      const platform_object& platform = db.get_platform_by_owner(u_9000_id);
      auto iter_profit = platform.period_profits.find(1);
      BOOST_CHECK(iter_profit != platform.period_profits.end());
      auto iter_reward_profit = iter_profit->second.rewards_profits.find(GRAPHENE_CORE_ASSET_AID);
      BOOST_CHECK(iter_reward_profit != iter_profit->second.rewards_profits.end());
      BOOST_CHECK(iter_reward_profit->second == 10 * 250 * prec);

      post_object post_obj = db.get_post_by_platform(u_9000_id, u_1001_id, 1);
      int64_t poster_earned = (post_obj.receiptors[u_1001_id].cur_ratio * uint128_t(100000000) / 10000).convert_to<int64_t>();
      int64_t platform_earned = 100000000 - poster_earned;

      auto act_1001 = db.get_account_statistics_by_uid(u_1001_id);
      BOOST_CHECK(act_1001.core_balance == poster_earned * 10);
      auto act_9000 = db.get_account_statistics_by_uid(u_9000_id);
      BOOST_CHECK(act_9000.core_balance == (platform_earned * 10 + 100000 * prec));

      for (auto a : reward_map)
      {
         auto act = db.get_account_statistics_by_uid(a.first);
         BOOST_CHECK(act.core_balance == (100000 - 1000) * prec);
      }

   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE(post_platform_reward_test)
{
   try{
      ACTORS((1001)(9000));

      flat_map<account_uid_type, fc::ecc::private_key> score_map1;
      flat_map<account_uid_type, fc::ecc::private_key> score_map2;
      actor(1003, 20, score_map1);
      actor(2003, 20, score_map2);

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      // Return number of core shares (times precision)
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };


      for (int i = 0; i < 5; ++i)
         add_csaf_for_account(genesis_state.initial_accounts.at(i).uid, 1000);
      transfer(committee_account, u_9000_id, _core(100000));
      generate_blocks(10);

      BOOST_TEST_MESSAGE("Turn on the reward mechanism, open content award and platform voted award");
      committee_update_global_content_parameter_item_type item;
      item.value = { 300, 300, 1000, 31536000, 10, 10000000000000, 10000000000000, 10000000000000, 1000, 100 };
      committee_proposal_create(genesis_state.initial_accounts.at(0).uid, { item }, 100, voting_opinion_type::opinion_for, 100, 100);
      for (int i = 1; i < 5; ++i)
         committee_proposal_vote(genesis_state.initial_accounts.at(i).uid, 1, voting_opinion_type::opinion_for);
      generate_blocks(89);

      collect_csaf_from_committee(u_9000_id, 1000);
      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", { u_9000_private_key });
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", { u_9000_private_key });
      account_auth_platform({ u_1001_private_key }, u_1001_id, u_9000_id, 10000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);

      post_operation::ext extensions;
      extensions.license_lid = 1;
      create_post({ u_1001_private_key, u_9000_private_key }, u_9000_id, u_1001_id, "", "", "", "",
         optional<account_uid_type>(),
         optional<account_uid_type>(),
         optional<post_pid_type>(),
         extensions);

      account_manage_operation::opt options;
      options.can_rate = true;
      for (auto a : score_map1)
      {
         //transfer(committee_account, a.first, _core(100000));
         collect_csaf_from_committee(a.first, 100);
         account_auth_platform({ a.second }, a.first, u_9000_id, 1000 * prec, 0x1F);
         account_manage(GRAPHENE_NULL_ACCOUNT_UID, a.first, options);
         auto b = db.get_account_by_uid(a.first);
      }
      for (auto a : score_map2)
      {
         //transfer(committee_account, a.first, _core(100000));
         collect_csaf_from_committee(a.first, 100);
         account_auth_platform({ a.second }, a.first, u_9000_id, 1000 * prec, 0x1F);
         account_manage(GRAPHENE_NULL_ACCOUNT_UID, a.first, options);
      }

      for (auto a : score_map1)
      {
         score_a_post({ a.second }, a.first, u_9000_id, u_1001_id, 1, 5, 50);
      }
      for (auto a : score_map2)
      {
         score_a_post({ a.second }, a.first, u_9000_id, u_1001_id, 1, -5, 10);
      }

      generate_blocks(100);

      uint128_t award_average = (uint128_t)10000000000000 * 300 / (86400 * 365);

      uint128_t post_earned = award_average;
      uint128_t score_earned = post_earned * GRAPHENE_DEFAULT_PLATFORM_RECEIPTS_RATIO / GRAPHENE_100_PERCENT;
      uint128_t receiptor_earned = post_earned - score_earned;
      uint64_t  poster_earned = (receiptor_earned * 7500 / 10000).convert_to<uint64_t>();
      auto poster_act = db.get_account_statistics_by_uid(u_1001_id);
      BOOST_CHECK(poster_act.core_balance == poster_earned);

      vector<score_id_type> scores;
      for (auto a : score_map1)
      {
         auto score_id = db.get_score(u_9000_id, u_1001_id, 1, a.first).id;
         scores.push_back(score_id);
      }
      for (auto a : score_map2)
      {
         auto score_id = db.get_score(u_9000_id, u_1001_id, 1, a.first).id;
         scores.push_back(score_id);
      }
      auto result = get_effective_csaf(scores, 50 * 20 + 10 * 20);
      share_type total_score_balance = 0;
      for (auto a : std::get<0>(result))
      {
         auto balance = score_earned.convert_to<uint64_t>() * std::get<1>(a) / std::get<1>(result);
         auto score_act = db.get_account_statistics_by_uid(std::get<0>(a));
         total_score_balance += balance;
         BOOST_CHECK(score_act.core_balance == balance);
      }

      auto platform_act = db.get_account_statistics_by_uid(u_9000_id);
      auto platform_core_balance = receiptor_earned.convert_to<uint64_t>() - poster_earned + award_average.convert_to<uint64_t>() + 10000000000;
      BOOST_CHECK(platform_act.core_balance == platform_core_balance);

      auto platform_obj = db.get_platform_by_owner(u_9000_id);
      auto post_profit = receiptor_earned.convert_to<uint64_t>() - poster_earned;
      auto iter_profit = platform_obj.period_profits.begin();
      BOOST_CHECK(iter_profit != platform_obj.period_profits.end());
      BOOST_CHECK(iter_profit->second.post_profits == post_profit);
      BOOST_CHECK(iter_profit->second.platform_profits == award_average.convert_to<uint64_t>());

      const auto& apt_idx = db.get_index_type<active_post_index>().indices().get<by_id>();
      auto active_post_obj = *(apt_idx.begin());
      BOOST_CHECK(active_post_obj.positive_win == true);
      BOOST_CHECK(active_post_obj.receiptor_details.at(u_1001_id).post_award == poster_earned);
      BOOST_CHECK(active_post_obj.post_award == (receiptor_earned.convert_to<uint64_t>() + total_score_balance));
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


//test api: process_platform_voted_awards()
BOOST_AUTO_TEST_CASE(platform_voted_awards_test)
{
   try{
      //ACTORS((1001)(9000));

      flat_map<account_uid_type, fc::ecc::private_key> platform_map1;
      flat_map<account_uid_type, fc::ecc::private_key> platform_map2;
      actor(8001, 5, platform_map1);
      actor(9001, 5, platform_map2);
      flat_set<account_uid_type> platform_set1;
      flat_set<account_uid_type> platform_set2;
      for (const auto&p : platform_map1)
         platform_set1.insert(p.first);
      for (const auto&p : platform_map2)
         platform_set2.insert(p.first);

      flat_map<account_uid_type, fc::ecc::private_key> vote_map1;
      flat_map<account_uid_type, fc::ecc::private_key> vote_map2;
      actor(1003, 10, vote_map1);
      actor(2003, 20, vote_map2);

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      // Return number of core shares (times precision)
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };


      for (int i = 0; i < 5; ++i)
         add_csaf_for_account(genesis_state.initial_accounts.at(i).uid, 1000);

      for (const auto &p : platform_map1)
      {
         transfer(committee_account, p.first, _core(100000));
         collect_csaf_from_committee(p.first, 1000);
      }
      for (const auto &p : platform_map2)
      {
         transfer(committee_account, p.first, _core(100000));
         collect_csaf_from_committee(p.first, 1000);
      }
      for (const auto &v : vote_map1)
      {
         transfer(committee_account, v.first, _core(10000));
         collect_csaf_from_committee(v.first, 1000);
      }
      for (const auto &v : vote_map2)
      {
         transfer(committee_account, v.first, _core(10000));
         collect_csaf_from_committee(v.first, 1000);
      }

      uint32_t i = 0;
      for (const auto &p : platform_map1)
      {
         create_platform(p.first, "platform" + i, _core(10000), "www.123456789.com" + i, "", { p.second });
         i++;
      }
      for (const auto &p : platform_map2)
      {
         create_platform(p.first, "platform" + i, _core(10000), "www.123456789.com" + i, "", { p.second });
         i++;
      }

      uint32_t current_block_num = db.head_block_num();

      BOOST_TEST_MESSAGE("Turn on the reward mechanism, open content award and platform voted award");
      committee_update_global_content_parameter_item_type content_item;
      content_item.value = { 300, 300, 1000, 31536000, 10, 0, 0, 10000000000000, 100, 10 };
      committee_proposal_create(genesis_state.initial_accounts.at(0).uid, { content_item }, current_block_num + 10, voting_opinion_type::opinion_for, current_block_num + 10, current_block_num + 10);
      committee_update_global_parameter_item_type item;
      item.value.governance_votes_update_interval = 20;
      committee_proposal_create(genesis_state.initial_accounts.at(0).uid, { item }, current_block_num + 10, voting_opinion_type::opinion_for, current_block_num + 10, current_block_num + 10);
      for (int i = 1; i < 5; ++i)
      {
         committee_proposal_vote(genesis_state.initial_accounts.at(i).uid, 1, voting_opinion_type::opinion_for);
         committee_proposal_vote(genesis_state.initial_accounts.at(i).uid, 2, voting_opinion_type::opinion_for);
      }

      generate_blocks(10);

      flat_set<account_uid_type> empty;
      for (const auto &v : vote_map1)
      {
         update_platform_votes(v.first, platform_set1, empty, { v.second });
      }
      for (const auto &v : vote_map2)
      {
         update_platform_votes(v.first, platform_set2, empty, { v.second });
      }

      generate_blocks(100);

      uint128_t award = (uint128_t)10000000000000 * 300 / (86400 * 365);
      uint128_t platform_award_basic = award * 2000 / 10000;
      uint128_t basic = platform_award_basic / (platform_map1.size() + platform_map2.size());
      uint128_t platform_award_by_votes = award - platform_award_basic;

      uint32_t total_vote = 46293 * (10 + 20) * 5;
      for (const auto&p : platform_map1)
      {
         uint32_t votes = 46293 * 10;
         uint128_t award_by_votes = platform_award_by_votes * votes / total_vote;
         share_type balance = (award_by_votes + basic).convert_to<uint64_t>();
         auto pla_act = db.get_account_statistics_by_uid(p.first);
         BOOST_CHECK(pla_act.core_balance == balance + 10000000000);
         auto platform_obj = db.get_platform_by_owner(p.first);
         BOOST_CHECK(platform_obj.vote_profits.begin()->second == balance);
      }
      for (const auto&p : platform_map2)
      {
         uint32_t votes = 46293 * 20;
         uint128_t award_by_votes = platform_award_by_votes * votes / total_vote;
         share_type balance = (award_by_votes + basic).convert_to<uint64_t>();
         auto pla_act = db.get_account_statistics_by_uid(p.first);
         BOOST_CHECK(pla_act.core_balance == balance + 10000000000);
         auto platform_obj = db.get_platform_by_owner(p.first);
         BOOST_CHECK(platform_obj.vote_profits.begin()->second == balance);
      }
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}


BOOST_AUTO_TEST_CASE(transfer_extension_test)
{
   try{
      ACTORS((1000)(1001)(2000)(9000));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);

      // Return number of core shares (times precision)
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_1001_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_1001_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);
      const account_statistics_object& temp = db.get_account_statistics_by_uid(u_1000_id);

      // make sure the database requires our fee to be nonzero
      enable_fees();

      flat_set<fc::ecc::private_key> sign_keys;
      sign_keys.insert(u_1000_private_key);
      transfer_extension(sign_keys, u_1000_id, u_1000_id, _core(6000), "", true, false);
      const account_statistics_object& ant1000 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(ant1000.prepaid == 6000 * prec);
      BOOST_CHECK(ant1000.core_balance == 4000 * prec);

      transfer_extension(sign_keys, u_1000_id, u_1001_id, _core(5000), "", false, true);
      const account_statistics_object& ant1000_1 = db.get_account_statistics_by_uid(u_1000_id);
      const account_statistics_object& ant1001 = db.get_account_statistics_by_uid(u_1001_id);
      BOOST_CHECK(ant1000_1.prepaid == 1000 * prec);
      BOOST_CHECK(ant1001.core_balance == 15000 * prec);

      flat_set<fc::ecc::private_key> sign_keys1;
      sign_keys1.insert(u_1001_private_key);
      transfer_extension(sign_keys1, u_1001_id, u_1000_id, _core(15000), "", true, true);
      const account_statistics_object& ant1000_2 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(ant1000_2.prepaid == 1000 * prec);
      BOOST_CHECK(ant1000_2.core_balance == 19000 * prec);

      transfer_extension(sign_keys, u_1000_id, u_1001_id, _core(1000), "", false, false);
      const account_statistics_object& ant1001_2 = db.get_account_statistics_by_uid(u_1001_id);
      const account_statistics_object& ant1000_3 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(ant1001_2.prepaid == 1000 * prec);
      BOOST_CHECK(ant1000_3.prepaid == 0);

      account_auth_platform({ u_2000_private_key }, u_2000_id, u_9000_id, 1000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Transfer |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      transfer_extension({ u_2000_private_key }, u_2000_id, u_2000_id, _core(10000), "", true, false);
      transfer_extension({ u_9000_private_key }, u_2000_id, u_9000_id, _core(1000), "", false, true);
      const account_statistics_object& ant2000 = db.get_account_statistics_by_uid(u_2000_id);
      const account_statistics_object& ant9000 = db.get_account_statistics_by_uid(u_9000_id);
      BOOST_CHECK(ant2000.prepaid == 9000 * prec);
      BOOST_CHECK(ant9000.core_balance == 1000 * prec);

   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(account_auth_platform_test)
{
   try{
      ACTORS((1000)(9000));
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_9000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);

      flat_set<fc::ecc::private_key> sign_keys;
      sign_keys.insert(u_9000_private_key);
      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", sign_keys);

      flat_set<fc::ecc::private_key> sign_keys1;
      sign_keys1.insert(u_1000_private_key);
      account_auth_platform(sign_keys1, u_1000_id, u_9000_id, 1000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Transfer |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);

      const account_auth_platform_object& ant1000 = db.get_account_auth_platform_object_by_account_platform(u_1000_id, u_9000_id);
      BOOST_CHECK(ant1000.max_limit == 1000 * prec);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Forward);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Liked);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Buyout);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Comment);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Reward);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Transfer);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Post);
      BOOST_CHECK(ant1000.permission_flags & account_auth_platform_object::Platform_Permission_Content_Update);


      account_auth_platform(sign_keys1, u_1000_id, u_9000_id, 6000 * prec, 0);

      const account_auth_platform_object& ant10001 = db.get_account_auth_platform_object_by_account_platform(u_1000_id, u_9000_id);
      BOOST_CHECK(ant10001.max_limit == 6000 * prec);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Forward) == 0);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Liked) == 0);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Buyout) == 0);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Comment) == 0);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Reward) == 0);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Transfer) == 0);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Post) == 0);
      BOOST_CHECK((ant10001.permission_flags & account_auth_platform_object::Platform_Permission_Content_Update) == 0);
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(license_test)
{
   try{
      ACTORS((1000)(9000));
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_9000_id, _core(10000));
      add_csaf_for_account(u_9000_id, 10000);

      flat_set<fc::ecc::private_key> sign_keys;
      sign_keys.insert(u_9000_private_key);
      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", sign_keys);

      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", sign_keys);

      const license_object& license = db.get_license_by_platform(u_9000_id, 1);
      BOOST_CHECK(license.license_type == 6);
      BOOST_CHECK(license.hash_value == "999999999");
      BOOST_CHECK(license.extra_data == "extra");
      BOOST_CHECK(license.title == "license title");
      BOOST_CHECK(license.body == "license body");
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(post_test)
{
   try{
      ACTORS((1000)(2000)(9000));
      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      transfer(committee_account, u_9000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);

      flat_set<fc::ecc::private_key> sign_keys;
      sign_keys.insert(u_9000_private_key);
      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", sign_keys);
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", sign_keys);

      flat_set<fc::ecc::private_key> sign_keys1;
      sign_keys1.insert(u_1000_private_key);
      account_auth_platform(sign_keys1, u_1000_id, u_9000_id, 1000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      sign_keys1.insert(u_9000_private_key);

      map<account_uid_type, Receiptor_Parameter> receiptors;
      receiptors.insert(std::make_pair(u_9000_id, Receiptor_Parameter{ GRAPHENE_DEFAULT_PLATFORM_RECEIPTS_RATIO, false, 0, 0 }));
      receiptors.insert(std::make_pair(u_1000_id, Receiptor_Parameter{ 5000, false, 0, 0 }));
      receiptors.insert(std::make_pair(u_2000_id, Receiptor_Parameter{ 2500, false, 0, 0 }));

      post_operation::ext extension;
      extension.post_type = post_operation::Post_Type_Post;
      extension.forward_price = 10000 * prec;
      extension.receiptors = receiptors;
      extension.license_lid = 1;
      extension.permission_flags = post_object::Post_Permission_Forward |
         post_object::Post_Permission_Liked |
         post_object::Post_Permission_Buyout |
         post_object::Post_Permission_Comment |
         post_object::Post_Permission_Reward;

      create_post(sign_keys1, u_9000_id, u_1000_id, "6666666", "document name", "document body", "extra", optional<account_uid_type>(), optional<account_uid_type>(), optional<post_pid_type>(), extension);

      const post_object& post = db.get_post_by_platform(u_9000_id, u_1000_id, 1);
      BOOST_CHECK(post.hash_value == "6666666");
      BOOST_CHECK(post.extra_data == "extra");
      BOOST_CHECK(post.title == "document name");
      BOOST_CHECK(post.body == "document body");
      BOOST_CHECK(post.forward_price == 10000 * prec);
      BOOST_CHECK(post.license_lid == 1);
      BOOST_CHECK(post.permission_flags == post_object::Post_Permission_Forward |
         post_object::Post_Permission_Liked |
         post_object::Post_Permission_Buyout |
         post_object::Post_Permission_Comment |
         post_object::Post_Permission_Reward);
      BOOST_CHECK(post.receiptors.find(u_9000_id) != post.receiptors.end());
      Receiptor_Parameter r9 = post.receiptors.find(u_9000_id)->second;
      BOOST_CHECK(r9 == Receiptor_Parameter(GRAPHENE_DEFAULT_PLATFORM_RECEIPTS_RATIO, false, 0, 0));
      BOOST_CHECK(post.receiptors.find(u_1000_id) != post.receiptors.end());
      Receiptor_Parameter r1 = post.receiptors.find(u_1000_id)->second;
      BOOST_CHECK(r1 == Receiptor_Parameter(5000, false, 0, 0));
      BOOST_CHECK(post.receiptors.find(u_2000_id) != post.receiptors.end());
      Receiptor_Parameter r2 = post.receiptors.find(u_2000_id)->second;
      BOOST_CHECK(r2 == Receiptor_Parameter(2500, false, 0, 0));

   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(comment_test)
{
   try{
      ACTORS((1000)(2000)(9000));
      account_manage(u_1000_id, account_manage_operation::opt{ true, true, true });
      account_manage(u_2000_id, account_manage_operation::opt{ true, true, true });

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      transfer(committee_account, u_9000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);

      flat_set<fc::ecc::private_key> sign_keys;
      sign_keys.insert(u_9000_private_key);
      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", sign_keys);
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", sign_keys);

      flat_set<fc::ecc::private_key> sign_keys1;
      flat_set<fc::ecc::private_key> sign_keys2;
      sign_keys1.insert(u_1000_private_key);
      sign_keys2.insert(u_2000_private_key);
      account_auth_platform(sign_keys1, u_1000_id, u_9000_id, 1000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      account_auth_platform(sign_keys2, u_2000_id, u_9000_id, 1000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      sign_keys1.insert(u_9000_private_key);
      sign_keys2.insert(u_9000_private_key);

      post_operation::ext extension;
      extension.post_type = post_operation::Post_Type_Post;
      extension.forward_price = 10000 * prec;
      extension.license_lid = 1;
      extension.permission_flags = post_object::Post_Permission_Forward |
         post_object::Post_Permission_Liked |
         post_object::Post_Permission_Buyout |
         post_object::Post_Permission_Comment |
         post_object::Post_Permission_Reward;

      create_post(sign_keys1, u_9000_id, u_1000_id, "6666666", "document name", "document body", "extra", optional<account_uid_type>(), optional<account_uid_type>(), optional<post_pid_type>(), extension);

      extension.post_type = post_operation::Post_Type_Comment;
      create_post(sign_keys2, u_9000_id, u_2000_id, "2333333", "comment", "the post is good", "extra", u_9000_id, u_1000_id, 1, extension);
      const post_object& comment = db.get_post_by_platform(u_9000_id, u_2000_id, 1);
      BOOST_CHECK(comment.origin_platform == u_9000_id);
      BOOST_CHECK(comment.origin_poster == u_1000_id);
      BOOST_CHECK(comment.origin_post_pid == 1);
      BOOST_CHECK(comment.hash_value == "2333333");
      BOOST_CHECK(comment.title == "comment");
      BOOST_CHECK(comment.body == "the post is good");
      BOOST_CHECK(comment.extra_data == "extra");
      BOOST_CHECK(comment.forward_price == 10000 * prec);
      BOOST_CHECK(comment.license_lid == 1);
      BOOST_CHECK(comment.permission_flags == post_object::Post_Permission_Forward |
         post_object::Post_Permission_Liked |
         post_object::Post_Permission_Buyout |
         post_object::Post_Permission_Comment |
         post_object::Post_Permission_Reward);
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(forward_test)
{
   try{
      ACTORS((1000)(2000)(9000)(9001));
      account_manage(u_1000_id, account_manage_operation::opt{ true, true, true });
      account_manage(u_2000_id, account_manage_operation::opt{ true, true, true });

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      generate_blocks(10);

      BOOST_TEST_MESSAGE("Turn on the reward mechanism, open content award and platform voted award");
      committee_update_global_content_parameter_item_type item;
      item.value = { 300, 300, 1000, 31536000, 10, 10000000000000, 10000000000000, 10000000000000, 1000, 100 };
      committee_proposal_create(genesis_state.initial_accounts.at(0).uid, { item }, 100, voting_opinion_type::opinion_for, 100, 100);
      for (int i = 1; i < 5; ++i)
         committee_proposal_vote(genesis_state.initial_accounts.at(i).uid, 1, voting_opinion_type::opinion_for);
      generate_blocks(89);

      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      transfer(committee_account, u_9000_id, _core(10000));
      transfer(committee_account, u_9001_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);
      add_csaf_for_account(u_9001_id, 10000);
      transfer_extension({ u_1000_private_key }, u_1000_id, u_1000_id, _core(10000), "", true, false);
      transfer_extension({ u_2000_private_key }, u_2000_id, u_2000_id, _core(10000), "", true, false);

      flat_set<fc::ecc::private_key> sign_keys;
      sign_keys.insert(u_9000_private_key);
      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", sign_keys);
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", sign_keys);
      flat_set<fc::ecc::private_key> sign_keys1;
      sign_keys1.insert(u_9001_private_key);
      create_platform(u_9001_id, "platform2", _core(10000), "www.655667669.com", "", sign_keys1);
      create_license(u_9001_id, 1, "7878787878", "license title", "license body", "extra", sign_keys1);


      flat_set<fc::ecc::private_key> sign_keys_1;
      flat_set<fc::ecc::private_key> sign_keys_2;
      sign_keys_1.insert(u_1000_private_key);
      sign_keys_2.insert(u_2000_private_key);
      account_auth_platform(sign_keys_1, u_1000_id, u_9000_id, 10000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      account_auth_platform(sign_keys_2, u_2000_id, u_9001_id, 10000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      sign_keys_1.insert(u_9000_private_key);
      sign_keys_2.insert(u_9001_private_key);
      bool do_by_platform = true; // modify to false , change to do_by_account
      if (do_by_platform){
         sign_keys_2.erase(u_2000_private_key);
      }

      post_operation::ext extension;
      extension.post_type = post_operation::Post_Type_Post;
      extension.forward_price = 10000 * prec;
      extension.license_lid = 1;
      extension.permission_flags = post_object::Post_Permission_Forward |
         post_object::Post_Permission_Liked |
         post_object::Post_Permission_Buyout |
         post_object::Post_Permission_Comment |
         post_object::Post_Permission_Reward;

      create_post(sign_keys_1, u_9000_id, u_1000_id, "6666666", "document name", "document body", "extra", optional<account_uid_type>(), optional<account_uid_type>(), optional<post_pid_type>(), extension);

      extension.post_type = post_operation::Post_Type_forward_And_Modify;
      create_post(sign_keys_2, u_9001_id, u_2000_id, "9999999", "new titile", "new body", "extra", u_9000_id, u_1000_id, 1, extension);

      const post_object& forward_post = db.get_post_by_platform(u_9001_id, u_2000_id, 1);
      BOOST_CHECK(forward_post.origin_platform == u_9000_id);
      BOOST_CHECK(forward_post.origin_poster == u_1000_id);
      BOOST_CHECK(forward_post.origin_post_pid == 1);
      BOOST_CHECK(forward_post.hash_value == "9999999");
      BOOST_CHECK(forward_post.title == "new titile");
      BOOST_CHECK(forward_post.body == "new body");
      BOOST_CHECK(forward_post.extra_data == "extra");
      BOOST_CHECK(forward_post.forward_price == 10000 * prec);
      BOOST_CHECK(forward_post.license_lid == 1);
      BOOST_CHECK(forward_post.permission_flags == post_object::Post_Permission_Forward |
         post_object::Post_Permission_Liked |
         post_object::Post_Permission_Buyout |
         post_object::Post_Permission_Comment |
         post_object::Post_Permission_Reward);

      const account_statistics_object& sobj1 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(sobj1.prepaid == 17500 * prec);
      const account_statistics_object& platform1 = db.get_account_statistics_by_uid(u_9000_id);
      BOOST_CHECK(platform1.prepaid == 2500 * prec);
      BOOST_CHECK(platform1.core_balance == 10000 * prec);
      const account_statistics_object& sobj2 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(sobj2.prepaid == 0);

      if (do_by_platform){
         //auto auth_data = sobj2.prepaids_for_platform.find(u_9001_id);
         //BOOST_CHECK(auth_data != sobj2.prepaids_for_platform.end());
         //BOOST_CHECK(auth_data->second.cur_used == 10000*prec);
         //BOOST_CHECK(sobj2.get_auth_platform_usable_prepaid(u_9001_id) == 0);

         auto auth_data = db.get_account_auth_platform_object_by_account_platform(u_2000_id, u_9001_id);
         BOOST_CHECK(auth_data.cur_used == 10000 * prec);
         BOOST_CHECK(auth_data.get_auth_platform_usable_prepaid(sobj2.prepaid) == 0);
      }

      const auto& apt_idx = db.get_index_type<active_post_index>().indices().get<by_post_pid>();
      auto apt_itr = apt_idx.find(std::make_tuple(u_9000_id, u_1000_id, 1, 1));
      BOOST_CHECK(apt_itr != apt_idx.end());
      auto active_post = *apt_itr;
      BOOST_CHECK(active_post.forward_award == 10000 * prec);
      auto iter_receiptor = active_post.receiptor_details.find(u_1000_id);
      BOOST_CHECK(iter_receiptor != active_post.receiptor_details.end());
      BOOST_CHECK(iter_receiptor->second.forward == 7500 * prec);
      auto iter_receiptor2 = active_post.receiptor_details.find(u_9000_id);
      BOOST_CHECK(iter_receiptor2 != active_post.receiptor_details.end());
      BOOST_CHECK(iter_receiptor2->second.forward == 2500 * prec);

      const platform_object& platform = db.get_platform_by_owner(u_9000_id);
      auto iter_profit = platform.period_profits.find(1);
      BOOST_CHECK(iter_profit != platform.period_profits.end());
      BOOST_CHECK(iter_profit->second.foward_profits == 2500 * prec);
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(buyout_test)
{
   try{
      ACTORS((1000)(2000)(9000));
      account_manage(u_1000_id, account_manage_operation::opt{ true, true, true });
      account_manage(u_2000_id, account_manage_operation::opt{ true, true, true });

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      transfer(committee_account, u_9000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);
      transfer_extension({ u_1000_private_key }, u_1000_id, u_1000_id, _core(10000), "", true, false);
      transfer_extension({ u_2000_private_key }, u_2000_id, u_2000_id, _core(10000), "", true, false);

      flat_set<fc::ecc::private_key> sign_keys;
      sign_keys.insert(u_9000_private_key);
      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", sign_keys);
      create_license(u_9000_id, 6, "999999999", "license title", "license body", "extra", sign_keys);


      flat_set<fc::ecc::private_key> sign_keys_1;
      flat_set<fc::ecc::private_key> sign_keys_2;
      sign_keys_1.insert(u_1000_private_key);
      sign_keys_2.insert(u_2000_private_key);
      account_auth_platform(sign_keys_1, u_1000_id, u_9000_id, 10000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      account_auth_platform(sign_keys_2, u_2000_id, u_9000_id, 10000 * prec, account_auth_platform_object::Platform_Permission_Forward |
         account_auth_platform_object::Platform_Permission_Liked |
         account_auth_platform_object::Platform_Permission_Buyout |
         account_auth_platform_object::Platform_Permission_Comment |
         account_auth_platform_object::Platform_Permission_Reward |
         account_auth_platform_object::Platform_Permission_Post |
         account_auth_platform_object::Platform_Permission_Content_Update);
      sign_keys_1.insert(u_9000_private_key);
      sign_keys_2.insert(u_9000_private_key);
      bool do_by_platform = true; // modify to false , change to do_by_account
      if (do_by_platform){
         sign_keys_2.erase(u_2000_private_key);
      }

      post_operation::ext extension;
      extension.post_type = post_operation::Post_Type_Post;
      extension.forward_price = 10000 * prec;
      extension.license_lid = 1;
      extension.permission_flags = post_object::Post_Permission_Forward |
         post_object::Post_Permission_Liked |
         post_object::Post_Permission_Buyout |
         post_object::Post_Permission_Comment |
         post_object::Post_Permission_Reward;

      create_post(sign_keys_1, u_9000_id, u_1000_id, "6666666", "document name", "document body", "extra", optional<account_uid_type>(), optional<account_uid_type>(), optional<post_pid_type>(), extension);

      post_update_operation::ext ext;
      ext.receiptor = u_1000_id;
      ext.to_buyout = true;
      ext.buyout_ratio = 3000;
      ext.buyout_price = 1000 * prec;
      ext.buyout_expiration = time_point_sec::maximum();
      update_post({ u_1000_private_key, u_9000_private_key }, u_9000_id, u_1000_id, 1, "", "", "", "", ext);

      buyout_post(u_2000_id, u_9000_id, u_1000_id, 1, u_1000_id, sign_keys_2);

      const post_object& post = db.get_post_by_platform(u_9000_id, u_1000_id, 1);
      auto iter1 = post.receiptors.find(u_1000_id);
      BOOST_CHECK(iter1 != post.receiptors.end());
      BOOST_CHECK(iter1->second.cur_ratio == 4500);
      BOOST_CHECK(iter1->second.to_buyout == false);
      BOOST_CHECK(iter1->second.buyout_ratio == 0);
      BOOST_CHECK(iter1->second.buyout_price == 0);

      auto iter2 = post.receiptors.find(u_2000_id);
      BOOST_CHECK(iter2 != post.receiptors.end());
      BOOST_CHECK(iter2->second.cur_ratio == 3000);
      BOOST_CHECK(iter2->second.to_buyout == false);
      BOOST_CHECK(iter2->second.buyout_ratio == 0);
      BOOST_CHECK(iter2->second.buyout_price == 0);

      const account_statistics_object& sobj1 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(sobj1.prepaid == 11000 * prec);
      const account_statistics_object& sobj2 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(sobj2.prepaid == 9000 * prec);

      if (do_by_platform){
         //auto auth_data = sobj2.prepaids_for_platform.find(u_9000_id);
         //BOOST_CHECK(auth_data != sobj2.prepaids_for_platform.end());
         //BOOST_CHECK(auth_data->second.cur_used == 1000 * prec);

         auto auth_data = db.get_account_auth_platform_object_by_account_platform(u_2000_id, u_9000_id);
         BOOST_CHECK(auth_data.cur_used == 1000 * prec);
      }
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(advertising_test)
{
   try{
      ACTORS((1000)(2000)(3000)(4000)(9000));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      transfer(committee_account, u_3000_id, _core(10000));
      transfer(committee_account, u_4000_id, _core(10000));
      transfer(committee_account, u_9000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_3000_id, 10000);
      add_csaf_for_account(u_4000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);

      create_platform(u_9000_id, "platform", _core(10000), "www.123456789.com", "", { u_9000_private_key });
      create_advertising({ u_9000_private_key }, u_9000_id, "this is a test", share_type(100000000), 100000);
      generate_blocks(10);
      const auto& idx = db.get_index_type<advertising_index>().indices().get<by_advertising_platform>();
      const auto& obj = *(idx.begin());
      BOOST_CHECK(obj.description == "this is a test");
      BOOST_CHECK(obj.unit_time == 100000);
      BOOST_CHECK(obj.unit_price.value == 100000000);

      buy_advertising({ u_1000_private_key }, u_1000_id, u_9000_id, 1, time_point_sec(1551752731), 2, "u_1000", "");
      buy_advertising({ u_2000_private_key }, u_2000_id, u_9000_id, 1, time_point_sec(1551752731), 2, "u_2000", "");
      buy_advertising({ u_3000_private_key }, u_3000_id, u_9000_id, 1, time_point_sec(1551752731), 2, "u_3000", "");
      buy_advertising({ u_4000_private_key }, u_4000_id, u_9000_id, 1, time_point_sec(1677911410), 2, "u_4000", "");

      const auto& idx_order = db.get_index_type<advertising_order_index>().indices().get<by_advertising_user_id>();
      auto itr1 = idx_order.lower_bound(u_1000_id);
      BOOST_CHECK(itr1 != idx_order.end());
      BOOST_CHECK(itr1->user == u_1000_id);
      BOOST_CHECK(itr1->released_balance == 100000000 * 2);
      BOOST_CHECK(itr1->start_time == time_point_sec(1551752731));

      auto itr2 = idx_order.lower_bound(u_2000_id);
      BOOST_CHECK(itr2 != idx_order.end());
      BOOST_CHECK(itr2->user == u_2000_id);
      BOOST_CHECK(itr2->released_balance == 100000000 * 2);
      BOOST_CHECK(itr2->start_time == time_point_sec(1551752731));

      auto itr3 = idx_order.lower_bound(u_3000_id);
      BOOST_CHECK(itr3 != idx_order.end());
      BOOST_CHECK(itr3->user == u_3000_id);
      BOOST_CHECK(itr3->released_balance == 100000000 * 2);
      BOOST_CHECK(itr3->start_time == time_point_sec(1551752731));

      auto itr4 = idx_order.lower_bound(u_4000_id);
      BOOST_CHECK(itr4 != idx_order.end());
      BOOST_CHECK(itr4->user == u_4000_id);
      BOOST_CHECK(itr4->released_balance == 100000000 * 2);
      BOOST_CHECK(itr4->start_time == time_point_sec(1677911410));

      const auto& user1 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(user1.core_balance == 8000 * prec);
      const auto& user2 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(user2.core_balance == 8000 * prec);
      const auto& user3 = db.get_account_statistics_by_uid(u_3000_id);
      BOOST_CHECK(user3.core_balance == 8000 * prec);
      const auto& user4 = db.get_account_statistics_by_uid(u_4000_id);
      BOOST_CHECK(user4.core_balance == 8000 * prec);

      confirm_advertising({ u_9000_private_key }, u_9000_id, obj.advertising_aid, 1, true);

      const auto& idx_ordered = db.get_index_type<advertising_order_index>().indices().get<by_advertising_order_state>();
      auto itr6 = idx_ordered.lower_bound(advertising_accepted);
      const advertising_order_object adobj1 = *itr6;
      BOOST_CHECK(itr6 != idx_ordered.end());
      BOOST_CHECK(itr6->user == u_1000_id);
      BOOST_CHECK(itr6->released_balance == 2000 * prec);
      BOOST_CHECK(itr6->start_time == time_point_sec(1551752731));

      confirm_advertising({ u_9000_private_key }, u_9000_id, obj.advertising_aid, 4, false);

      BOOST_CHECK(user1.core_balance == 8000 * prec);
      BOOST_CHECK(user2.core_balance == 10000 * prec);
      BOOST_CHECK(user3.core_balance == 10000 * prec);
      BOOST_CHECK(user4.core_balance == 10000 * prec);

      const auto& platform = db.get_account_statistics_by_uid(u_9000_id);
      BOOST_CHECK(platform.core_balance == (12000 - 20) * prec);

      update_advertising({ u_9000_private_key }, u_9000_id, obj.advertising_aid, "this is advertising test", share_type(200000000), 100000, optional<bool>());


      BOOST_CHECK(obj.description == "this is advertising test");
      BOOST_CHECK(obj.unit_time == 100000);
      BOOST_CHECK(obj.unit_price.value == 200000000);

      const auto& idx_by_clear_time = db.get_index_type<advertising_order_index>().indices().get<by_clear_time>();
      //auto itr_by_clear_time = 
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(custom_vote_test)
{
   try{
      ACTORS((1000)(2000)(3000)(4000)(9000));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      transfer(committee_account, u_3000_id, _core(10000));
      transfer(committee_account, u_4000_id, _core(10000));
      transfer(committee_account, u_9000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_3000_id, 10000);
      add_csaf_for_account(u_4000_id, 10000);
      add_csaf_for_account(u_9000_id, 10000);


      create_custom_vote({ u_9000_private_key }, u_9000_id, 1, "title", "description", time_point_sec(1560096000),
         0, share_type(1000000), 1, 3, { "aa", "bb", "cc", "dd" });


      //***************************************************
      //must start non_consensus custom vote, or check error
      //***************************************************
      const auto& idx = db.get_index_type<custom_vote_index>().indices().get<by_id>();
      const auto& obj = *(idx.begin());
      BOOST_CHECK(obj.custom_vote_creater == u_9000_id);
      BOOST_CHECK(obj.vote_vid == 1);
      BOOST_CHECK(obj.title == "title");
      BOOST_CHECK(obj.description == "description");
      BOOST_CHECK(obj.vote_expired_time == time_point_sec(1560096000));
      BOOST_CHECK(obj.required_asset_amount.value == 1000000);
      BOOST_CHECK(obj.vote_asset_id == 0);
      BOOST_CHECK(obj.minimum_selected_items == 1);
      BOOST_CHECK(obj.maximum_selected_items == 3);
      BOOST_CHECK(obj.options.size() == 4);
      BOOST_CHECK(obj.options.at(0) == "aa");
      BOOST_CHECK(obj.options.at(1) == "bb");
      BOOST_CHECK(obj.options.at(2) == "cc");
      BOOST_CHECK(obj.options.at(3) == "dd");

      cast_custom_vote({ u_1000_private_key }, u_1000_id, u_9000_id, 1, { 0, 1 });
      BOOST_CHECK(obj.vote_result.at(0) == 10000 * prec);
      BOOST_CHECK(obj.vote_result.at(1) == 10000 * prec);
      BOOST_CHECK(obj.vote_result.at(2) == 0);
      BOOST_CHECK(obj.vote_result.at(3) == 0);

      cast_custom_vote({ u_2000_private_key }, u_2000_id, u_9000_id, 1, { 0, 1, 2 });
      BOOST_CHECK(obj.vote_result.at(0) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(1) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(2) == 10000 * prec);
      BOOST_CHECK(obj.vote_result.at(3) == 0);

      cast_custom_vote({ u_3000_private_key }, u_3000_id, u_9000_id, 1, { 2, 3 });
      BOOST_CHECK(obj.vote_result.at(0) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(1) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(2) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(3) == 10000 * prec);

      cast_custom_vote({ u_4000_private_key }, u_4000_id, u_9000_id, 1, { 1, 3 });
      BOOST_CHECK(obj.vote_result.at(0) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(1) == 30000 * prec);
      BOOST_CHECK(obj.vote_result.at(2) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(3) == 20000 * prec);

      transfer(committee_account, u_1000_id, _core(40000));
      BOOST_CHECK(obj.vote_result.at(0) == 60000 * prec);
      BOOST_CHECK(obj.vote_result.at(1) == 70000 * prec);
      BOOST_CHECK(obj.vote_result.at(2) == 20000 * prec);
      BOOST_CHECK(obj.vote_result.at(3) == 20000 * prec);

      transfer(u_3000_id, u_1000_id, _core(5000));
      BOOST_CHECK(obj.vote_result.at(0) == 65000 * prec);
      BOOST_CHECK(obj.vote_result.at(1) == 75000 * prec);
      BOOST_CHECK(obj.vote_result.at(2) == 15000 * prec);
      BOOST_CHECK(obj.vote_result.at(3) == 15000 * prec);

   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(balance_lock_for_feepoint_test)
{
   try{
      ACTORS((1000)(2000));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_1000_id, _core(10000));
      transfer(committee_account, u_2000_id, _core(10000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);


      balance_lock_update({ u_1000_private_key }, u_1000_id, 5000 * prec);
      balance_lock_update({ u_2000_private_key }, u_2000_id, 8000 * prec);
      const auto& user1 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(user1.locked_balance_for_feepoint == 5000 * prec);
      BOOST_CHECK(user1.releasing_locked_feepoint == 0 * prec);
      const auto& user2 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(user2.locked_balance_for_feepoint == 8000 * prec);
      BOOST_CHECK(user2.releasing_locked_feepoint == 0 * prec);

      balance_lock_update({ u_1000_private_key }, u_1000_id, 6000 * prec);
      balance_lock_update({ u_2000_private_key }, u_2000_id, 5000 * prec);
      const auto& user3 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(user3.locked_balance_for_feepoint == 6000 * prec);
      BOOST_CHECK(user3.releasing_locked_feepoint == -1000 * prec);
      BOOST_CHECK(user3.feepoint_unlock_block_number == GRAPHENE_DEFAULT_UNLOCKED_BALANCE_RELEASE_DELAY);
      const auto& user4 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(user4.locked_balance_for_feepoint == 5000 * prec);
      BOOST_CHECK(user4.releasing_locked_feepoint == 3000 * prec);
      BOOST_CHECK(user4.feepoint_unlock_block_number == GRAPHENE_DEFAULT_UNLOCKED_BALANCE_RELEASE_DELAY);

      generate_blocks(GRAPHENE_DEFAULT_UNLOCKED_BALANCE_RELEASE_DELAY);
      generate_block(5);

      const auto& user5 = db.get_account_statistics_by_uid(u_1000_id);
      BOOST_CHECK(user5.locked_balance_for_feepoint == 6000 * prec);
      BOOST_CHECK(user5.releasing_locked_feepoint == 0 * prec);
      BOOST_CHECK(user5.feepoint_unlock_block_number == -1);
      const auto& user6 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(user6.locked_balance_for_feepoint == 5000 * prec);
      BOOST_CHECK(user6.releasing_locked_feepoint == 0 * prec);
      BOOST_CHECK(user6.feepoint_unlock_block_number == -1);

   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(total_witness_pledge_test)
{
   try{
      ACTORS((1000)(2000)(9001)(9002)(9003));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };
      transfer(committee_account, u_9001_id, _core(10000));
      transfer(committee_account, u_9002_id, _core(10000));
      transfer(committee_account, u_9003_id, _core(10000));
      add_csaf_for_account(u_9001_id, 10000);
      add_csaf_for_account(u_9002_id, 10000);
      add_csaf_for_account(u_9003_id, 10000);
      create_witness(u_9001_id, u_9001_private_key, _core(10000));
      create_witness(u_9002_id, u_9002_private_key, _core(10000));
      create_witness(u_9003_id, u_9003_private_key, _core(10000));


      transfer(committee_account, u_1000_id, _core(30000));
      transfer(committee_account, u_2000_id, _core(30000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);

      const witness_object& witness1 = create_witness(u_1000_id, u_1000_private_key, _core(10000));
      const witness_object& witness2 = create_witness(u_2000_id, u_2000_private_key, _core(10000));
      BOOST_CHECK(witness1.pledge == 10000 * prec);
      BOOST_CHECK(witness2.pledge == 10000 * prec);

      const dynamic_global_property_object dpo = db.get_dynamic_global_properties();
      BOOST_CHECK(dpo.total_witness_pledge == 50000 * prec);
      BOOST_CHECK(dpo.resign_witness_pledge_before_05 == 0);

      update_witness({ u_1000_private_key }, u_1000_id, optional<public_key_type>(), _core(15000), optional<string>());
      update_witness({ u_2000_private_key }, u_2000_id, optional<public_key_type>(), _core(15000), optional<string>());
      generate_blocks(28800);

      const dynamic_global_property_object dpo1 = db.get_dynamic_global_properties();
      BOOST_CHECK(dpo1.total_witness_pledge == 60000 * prec);
      BOOST_CHECK(dpo1.resign_witness_pledge_before_05 == 0);

      update_witness({ u_1000_private_key }, u_1000_id, optional<public_key_type>(), _core(0), optional<string>());
      update_witness({ u_2000_private_key }, u_2000_id, optional<public_key_type>(), _core(20000), optional<string>());
      const dynamic_global_property_object dpo2 = db.get_dynamic_global_properties();
      BOOST_CHECK(dpo2.total_witness_pledge == 65000 * prec);
      BOOST_CHECK(dpo2.resign_witness_pledge_before_05 == (-15000)*prec);

      generate_blocks(28800);

      const dynamic_global_property_object dpo3 = db.get_dynamic_global_properties();
      BOOST_CHECK(dpo3.total_witness_pledge == 65000 * prec);
      BOOST_CHECK(dpo3.resign_witness_pledge_before_05 == (-15000)*prec);
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(csaf_compute_test)
{
   try{
      ACTORS((1000));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      transfer(committee_account, u_1000_id, _core(3000000));

      //before_hardfork_01
      generate_blocks(1000);
      collect_csaf_origin({ u_1000_private_key }, u_1000_id, u_1000_id, 1);
      //debug in funs : _apply_block and compute_coin_seconds_earned

      //after_hardfork_04
      generate_blocks(28800);
      collect_csaf_origin({ u_1000_private_key }, u_1000_id, u_1000_id, 1);
      //debug in funs : _apply_block and compute_coin_seconds_earned

      //after_hardfork_05
      generate_blocks(28800);
      collect_csaf_origin({ u_1000_private_key }, u_1000_id, u_1000_id, 1);
      //debug in funs : _apply_block and compute_coin_seconds_earned
      
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(csaf_lease_test)
{
   try{
      ACTORS((1000)(2000));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      transfer(committee_account, u_1000_id, _core(30000));
      transfer(committee_account, u_2000_id, _core(30000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);

      csaf_lease({ u_1000_private_key }, u_1000_id, u_2000_id, 15000, time_point_sec(1562224400));
      const account_statistics_object ant_1000 = db.get_account_statistics_by_uid(u_1000_id);
      const account_statistics_object ant_2000 = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(ant_1000.core_leased_out == 15000*prec);
      BOOST_CHECK(ant_2000.core_leased_in == 15000 * prec);

      generate_blocks(28800);
      const account_statistics_object ant_1000a = db.get_account_statistics_by_uid(u_1000_id);
      const account_statistics_object ant_2000a = db.get_account_statistics_by_uid(u_2000_id);
      BOOST_CHECK(ant_1000a.core_leased_out == 0 * prec);
      BOOST_CHECK(ant_2000a.core_leased_in == 0 * prec);

   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(limit_order_test)
{
   try{
      ACTORS((1000)(2000)(3000)(1001)(1002)(2001)(2002)(3001)(3002));

      const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
      auto _core = [&](int64_t x) -> asset
      {  return asset(x*prec);    };

      transfer(committee_account, u_1000_id, _core(30000));
      transfer(committee_account, u_2000_id, _core(30000));
      transfer(committee_account, u_3000_id, _core(30000));
      add_csaf_for_account(u_1000_id, 10000);
      add_csaf_for_account(u_2000_id, 10000);
      add_csaf_for_account(u_3000_id, 10000);
      const account_object& ant_obj1= db.get_account_by_uid(u_1000_id);
      db.modify(ant_obj1, [&](account_object& s) {
         s.reg_info.registrar = u_1001_id;
         s.reg_info.referrer = u_1002_id;
         s.reg_info.registrar_percent = 60*GRAPHENE_1_PERCENT;
         s.reg_info.referrer_percent = 40*GRAPHENE_1_PERCENT;
      });
      const account_object& ant_obj2 = db.get_account_by_uid(u_2000_id);
      db.modify(ant_obj2, [&](account_object& s) {
         s.reg_info.registrar = u_2001_id;
         s.reg_info.referrer = u_2002_id;
         s.reg_info.registrar_percent = 60 * GRAPHENE_1_PERCENT;
         s.reg_info.referrer_percent = 40 * GRAPHENE_1_PERCENT;
      });
      const account_object& ant_obj3 = db.get_account_by_uid(u_3000_id);
      db.modify(ant_obj3, [&](account_object& s) {
         s.reg_info.registrar = u_3001_id;
         s.reg_info.referrer = u_3002_id;
         s.reg_info.registrar_percent = 60 * GRAPHENE_1_PERCENT;
         s.reg_info.referrer_percent = 40 * GRAPHENE_1_PERCENT;
      });

      asset_options options;
      options.max_supply = 100000000*prec;
      options.market_fee_percent = 1*GRAPHENE_1_PERCENT;
      options.max_market_fee = 20*prec;
      options.issuer_permissions = 15;
      options.flags = charge_market_fee;
      //options.whitelist_authorities = ;
      //options.blacklist_authorities = ;
      //options.whitelist_markets = ;
      //options.blacklist_markets = ;
      options.description = "test asset";
      options.extensions = graphene::chain::extension<additional_asset_options>();
      additional_asset_options exts;
      exts.reward_percent = 50 * GRAPHENE_1_PERCENT;
      options.extensions->value = exts;

      const account_object commit_obj = db.get_account_by_uid(committee_account);

      create_asset({ u_1000_private_key }, u_1000_id, "ABC", 5, options, share_type(100000000 * prec));
      const asset_object& ast = db.get_asset_by_aid(1);
      BOOST_CHECK(ast.symbol == "ABC");
      BOOST_CHECK(ast.precision == 5);
      BOOST_CHECK(ast.issuer == u_1000_id);
      BOOST_CHECK(ast.options.max_supply == 100000000 * prec);
      BOOST_CHECK(ast.options.flags == charge_market_fee);
      asset ast1 = db.get_balance(u_1000_id, 1);
      BOOST_CHECK(ast1.asset_id == 1);
      BOOST_CHECK(ast1.amount == 100000000 * prec);

      create_asset({ u_3000_private_key }, u_3000_id, "CBA", 5, options, share_type(100000000 * prec));
      const asset_object& ast2 = db.get_asset_by_aid(2);
      BOOST_CHECK(ast2.symbol == "CBA");
      BOOST_CHECK(ast2.precision == 5);
      BOOST_CHECK(ast2.issuer == u_3000_id);
      BOOST_CHECK(ast2.options.max_supply == 100000000 * prec);
      BOOST_CHECK(ast2.options.flags == charge_market_fee);
      asset ast7 = db.get_balance(u_3000_id, 2);
      BOOST_CHECK(ast7.asset_id == 2);
      BOOST_CHECK(ast7.amount == 100000000 * prec);

      create_limit_order({ u_1000_private_key }, u_1000_id, 1, 10000*prec, 0, 1000*prec, 1569859200, false);
      create_limit_order({ u_2000_private_key }, u_2000_id, 0, 1000*prec, 1, 10000*prec, 1569859200, false);

      asset ast21 = db.get_balance(u_1000_id, 1);
      BOOST_CHECK(ast21.asset_id == 1);
      BOOST_CHECK(ast21.amount == 99990000 * prec);
      asset ast3 = db.get_balance(u_2000_id, 1);
      BOOST_CHECK(ast3.asset_id == 1);
      BOOST_CHECK(ast3.amount == 9980 * prec);

      const account_statistics_object& aso1 = db.get_account_statistics_by_uid(u_2001_id);
      auto iter1 = aso1.uncollected_market_fees.find(1);
      BOOST_CHECK(iter1 != aso1.uncollected_market_fees.end());
      BOOST_CHECK(iter1->second == 6 * prec);
      const account_statistics_object& aso2 = db.get_account_statistics_by_uid(u_2002_id);
      auto iter2 = aso2.uncollected_market_fees.find(1);
      BOOST_CHECK(iter2 != aso2.uncollected_market_fees.end());
      BOOST_CHECK(iter2->second == 4 * prec);

      const asset_dynamic_data_object& ast_dy_obj = db.get_asset_by_aid(1).dynamic_asset_data_id(db);
      BOOST_CHECK(ast_dy_obj.accumulated_fees == 10 * prec);


      collect_market_fee({ u_2001_private_key }, u_2001_id, 1, 6*prec);
      collect_market_fee({ u_2002_private_key }, u_2002_id, 1, 4*prec);
      asset ast4 = db.get_balance(u_2001_id, 1);
      BOOST_CHECK(ast4.asset_id == 1);
      BOOST_CHECK(ast4.amount == 6 * prec);
      asset ast5 = db.get_balance(u_2002_id, 1);
      BOOST_CHECK(ast5.asset_id == 1);
      BOOST_CHECK(ast5.amount == 4 * prec);

      asset_claim_fees({u_1000_private_key}, u_1000_id, 1, 10*prec);
      const asset_dynamic_data_object& ast_dy_obj2 = db.get_asset_by_aid(1).dynamic_asset_data_id(db);
      BOOST_CHECK(ast_dy_obj2.accumulated_fees == 0);
      asset ast6 = db.get_balance(u_1000_id, 1);
      BOOST_CHECK(ast6.asset_id == 1);
      BOOST_CHECK(ast6.amount == 99990010 * prec);

      ///////////////////////////////////////////////////////////////////////////////////////

      create_limit_order({ u_1000_private_key }, u_1000_id, 1, 10000 * prec, 2, 20000 * prec, 1569859200, false);
      create_limit_order({ u_3000_private_key }, u_3000_id, 2, 20000 * prec, 1, 10000 * prec, 1569859200, false);

      asset ast8 = db.get_balance(u_1000_id, 1);
      BOOST_CHECK(ast8.asset_id == 1);
      BOOST_CHECK(ast8.amount == 99980010 * prec);
      asset ast9 = db.get_balance(u_1000_id, 2);
      BOOST_CHECK(ast9.asset_id == 2);
      BOOST_CHECK(ast9.amount == 19980 * prec);

      asset ast81 = db.get_balance(u_3000_id, 1);
      BOOST_CHECK(ast81.asset_id == 1);
      BOOST_CHECK(ast81.amount == 9980 * prec);
      asset ast91 = db.get_balance(u_3000_id, 2);
      BOOST_CHECK(ast91.asset_id == 2);
      BOOST_CHECK(ast91.amount == 99980000 * prec);

      const asset_dynamic_data_object& ast_dy_obj3 = db.get_asset_by_aid(1).dynamic_asset_data_id(db);
      BOOST_CHECK(ast_dy_obj3.accumulated_fees == 10 * prec);
      const asset_dynamic_data_object& ast_dy_obj4 = db.get_asset_by_aid(2).dynamic_asset_data_id(db);
      BOOST_CHECK(ast_dy_obj4.accumulated_fees == 10 * prec);

      const account_statistics_object& aso3 = db.get_account_statistics_by_uid(u_3001_id);
      auto iter3 = aso3.uncollected_market_fees.find(1);
      BOOST_CHECK(iter3 != aso3.uncollected_market_fees.end());
      BOOST_CHECK(iter3->second == 6 * prec);
      const account_statistics_object& aso4 = db.get_account_statistics_by_uid(u_3002_id);
      auto iter4 = aso4.uncollected_market_fees.find(1);
      BOOST_CHECK(iter4 != aso4.uncollected_market_fees.end());
      BOOST_CHECK(iter4->second == 4 * prec);

      const account_statistics_object& aso5 = db.get_account_statistics_by_uid(u_1001_id);
      auto iter5 = aso5.uncollected_market_fees.find(2);
      BOOST_CHECK(iter5 != aso5.uncollected_market_fees.end());
      BOOST_CHECK(iter5->second == 6 * prec);
      const account_statistics_object& aso6 = db.get_account_statistics_by_uid(u_1002_id);
      auto iter6 = aso6.uncollected_market_fees.find(2);
      BOOST_CHECK(iter6 != aso6.uncollected_market_fees.end());
      BOOST_CHECK(iter6->second == 4 * prec);
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_CASE(pledge_mining)
{
   try{


      {
         ACTORS((1000)(2000)(3001));

         const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
         auto _core = [&](int64_t x) -> asset
         {  return asset(x*prec);    };
         transfer(committee_account, u_1000_id, _core(600000));
         transfer(committee_account, u_2000_id, _core(600000));

         transfer(committee_account, u_3001_id, _core(600000));

         add_csaf_for_account(u_1000_id, 10000);
         add_csaf_for_account(u_2000_id, 10000);

         //test create witness extension
         graphene::chain::pledge_mining::ext ext;
         ext.can_pledge = true;
         ext.bonus_rate = 6000;
         create_witness({ u_1000_private_key }, u_1000_id, "test pledge witness-1", 500000 * prec, u_1000_public_key, ext);
         //create_witness({ u_2000_private_key }, u_2000_id, "test pledge witness-2", 500000 * prec, u_2000_public_key);

         //check witness object
         auto witness_obj = db.get_witness_by_uid(u_1000_id);
         BOOST_CHECK(witness_obj.can_pledge == true);
         BOOST_CHECK(witness_obj.bonus_rate == 6000);

         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_1000_id, 200000 * prec.value);
         generate_blocks(21);
         //check pledge mining object
         const pledge_mining_object& pledge_mining_obj = db.get_pledge_mining(u_1000_id, u_3001_id);
         BOOST_CHECK(pledge_mining_obj.pledge == 200000 * prec);

         auto last_pledge_witness_pay = db.get_dynamic_global_properties().by_pledge_witness_pay_per_block;

         uint32_t last_produce_blocks = 0;
         share_type total_bonus = 0;
         share_type witness_pay = 0;
         for (int i = 0; i < 10; ++i)
         {
            auto wit = db.get_witness_by_uid(u_1000_id);
            auto need_block_num = wit.last_update_bonus_block_num + 10000 - db.head_block_num();
            generate_blocks(need_block_num);
            wit = db.get_witness_by_uid(u_1000_id);
            const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();

            share_type pledge_bonus = ((fc::bigint)dpo.by_pledge_witness_pay_per_block.value * wit.bonus_rate * wit.total_mining_pledge
               / ((wit.pledge + wit.total_mining_pledge) * GRAPHENE_100_PERCENT)).to_int64();
            auto produce_blocks_per_cycle = wit.total_produced - last_produce_blocks;
            last_produce_blocks = wit.total_produced;

            share_type bonus_per_pledge = ((fc::uint128_t)(produce_blocks_per_cycle * pledge_bonus).value * GRAPHENE_PLEDGE_BONUS_PRECISION
               / wit.total_mining_pledge).to_uint64();

            total_bonus += ((fc::uint128_t)bonus_per_pledge.value * pledge_mining_obj.pledge.value
               / GRAPHENE_PLEDGE_BONUS_PRECISION).to_uint64();
            auto account = db.get_account_statistics_by_uid(u_3001_id);
            BOOST_CHECK(account.uncollected_pledge_bonus == total_bonus);
            auto witness = db.get_account_statistics_by_uid(u_1000_id);
            witness_pay = dpo.by_pledge_witness_pay_per_block*wit.total_produced - total_bonus;
            BOOST_CHECK(witness.uncollected_witness_pay == witness_pay);
         }

         //test update mining pledge
         flat_map<account_uid_type, fc::ecc::private_key> mining_map;
         actor(8001, 20, mining_map);
         for (const auto& m : mining_map)
         {
            transfer(committee_account, m.first, _core(600000));
            add_csaf_for_account(m.first, 10000);
            update_mining_pledge({ m.second }, m.first, u_1000_id, 200000 * prec.value);
         }


         auto wit = db.get_witness_by_uid(u_1000_id);
         auto produce_block = wit.total_produced;
         bool is_produce_block = false;
         for (int i = 0; i < 20; ++i)
         {
            generate_block();
            const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();
            if (last_pledge_witness_pay != dpo.by_pledge_witness_pay_per_block)
            {
               auto witness = db.get_witness_by_uid(u_1000_id);
               if (produce_block != db.get_witness_by_uid(u_1000_id).total_produced)
                  is_produce_block = true;
               break;
            }
         }


         //more account pledge mining to witness
         share_type total_bonus2 = 0;
         share_type witness_pay2 = 0;
         share_type bonus_3001 = 0;
         vector<share_type> total_bonus_per_account(mining_map.size());
         for (int i = 0; i < 10; ++i)
         {
            auto wit = db.get_witness_by_uid(u_1000_id);
            auto need_block_num = wit.last_update_bonus_block_num + 10000 - db.head_block_num();
            generate_blocks(need_block_num);
            wit = db.get_witness_by_uid(u_1000_id);
            const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();

            share_type pledge_bonus = ((fc::bigint)dpo.by_pledge_witness_pay_per_block.value * wit.bonus_rate * wit.total_mining_pledge
               / ((wit.pledge + wit.total_mining_pledge) * GRAPHENE_100_PERCENT)).to_int64();
            auto produce_blocks_per_cycle = wit.total_produced - last_produce_blocks;
            last_produce_blocks = wit.total_produced;

            share_type bonus_per_pledge = 0;
            if (i == 0 && is_produce_block)
            {
               share_type last_bonus = ((fc::bigint)last_pledge_witness_pay.value * wit.bonus_rate * wit.total_mining_pledge
                  / ((wit.pledge + wit.total_mining_pledge) * GRAPHENE_100_PERCENT)).to_int64();

               bonus_per_pledge = ((fc::uint128_t)((produce_blocks_per_cycle - 1) * pledge_bonus + last_bonus).value * GRAPHENE_PLEDGE_BONUS_PRECISION
                  / wit.total_mining_pledge).to_uint64();
            }
            else
               bonus_per_pledge = ((fc::uint128_t)(produce_blocks_per_cycle * pledge_bonus).value * GRAPHENE_PLEDGE_BONUS_PRECISION
               / wit.total_mining_pledge).to_uint64();

            int j = 0;
            for (const auto& m : mining_map)
            {
               auto mining_obj = db.get_pledge_mining(u_1000_id, u_3001_id);
               share_type bonus = ((fc::uint128_t)bonus_per_pledge.value * mining_obj.pledge.value
                  / GRAPHENE_PLEDGE_BONUS_PRECISION).to_uint64();
               total_bonus2 += bonus;
               total_bonus_per_account.at(j) += bonus;
               auto account = db.get_account_statistics_by_uid(m.first);
               BOOST_CHECK(account.uncollected_pledge_bonus == total_bonus_per_account.at(j));
               ++j;
               bonus_3001 = bonus;
            }
            total_bonus2 += bonus_3001;

            uint32_t block_num = wit.total_produced - produce_block;
            if (is_produce_block)
            {
               witness_pay2 = dpo.by_pledge_witness_pay_per_block*(block_num - 1) + last_pledge_witness_pay - total_bonus2;
            }
            else
               witness_pay2 = dpo.by_pledge_witness_pay_per_block*block_num - total_bonus2;
            auto witness = db.get_account_statistics_by_uid(u_1000_id);
            auto uncollected_witness_pay = witness_pay + witness_pay2;
            BOOST_CHECK(witness.uncollected_witness_pay == uncollected_witness_pay);
         }

         last_produce_blocks = db.get_witness_by_uid(u_1000_id).total_produced;
         auto witness_uncollet_pay = db.get_account_statistics_by_uid(u_1000_id).uncollected_witness_pay;

         auto last_account2_3001_bonus = db.get_account_statistics_by_uid(u_3001_id).uncollected_pledge_bonus;
         generate_blocks(5000);

         //cancel mining pledge
         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_1000_id, 0);
         auto wit3 = db.get_witness_by_uid(u_1000_id);
         const dynamic_global_property_object& dpo3 = db.get_dynamic_global_properties();
         share_type pledge_bonus3 = ((fc::bigint)dpo3.by_pledge_witness_pay_per_block.value * wit3.bonus_rate * (wit3.total_mining_pledge + 200000 * prec.value)
            / ((wit3.pledge + wit3.total_mining_pledge + 200000 * prec.value) * GRAPHENE_100_PERCENT)).to_int64();
         share_type bonus_per_pledge3 = ((fc::uint128_t)((wit3.total_produced - last_produce_blocks) * pledge_bonus3).value * GRAPHENE_PLEDGE_BONUS_PRECISION
            / wit.total_mining_pledge).to_uint64();
         share_type bonus3_3001 = ((fc::uint128_t)bonus_per_pledge3.value * 200000 * prec.value
            / GRAPHENE_PLEDGE_BONUS_PRECISION).to_uint64();
         auto account3_3001 = db.get_account_statistics_by_uid(u_3001_id);
         share_type uncollected_pledge_bonus_3001 = bonus3_3001 + last_account2_3001_bonus;
         BOOST_CHECK(account3_3001.uncollected_pledge_bonus == uncollected_pledge_bonus_3001);

         int k = 0;
         for (const auto&p : mining_map)
         {
            //auto mining_obj = db.get_pledge_mining(u_1000_id, u_3001_id);
            update_mining_pledge({ p.second }, p.first, u_1000_id, 0);
            auto account3 = db.get_account_statistics_by_uid(p.first);
            BOOST_CHECK(account3.uncollected_pledge_bonus == bonus3_3001 + total_bonus_per_account.at(k));
            ++k;
         }

         auto total_bonus_3 = bonus3_3001 * (mining_map.size() + 1);
         auto uncollet_witness_pay3 = dpo3.by_pledge_witness_pay_per_block.value * (wit3.total_produced - last_produce_blocks) - total_bonus_3;
         auto wit3_pay = db.get_account_statistics_by_uid(u_1000_id).uncollected_witness_pay;
         BOOST_CHECK(wit3_pay == uncollet_witness_pay3 + witness_uncollet_pay);

         generate_blocks(6000);
         auto wit4 = db.get_witness_by_uid(u_1000_id);
         BOOST_CHECK(wit4.total_mining_pledge == 0);
         BOOST_CHECK(wit4.bonus_per_pledge.size() == 0);
         BOOST_CHECK(wit4.unhandled_bonus == 0);
         BOOST_CHECK(wit4.need_distribute_bonus == 0);
         BOOST_CHECK(wit4.already_distribute_bonus == 0);

         //test release_mining_pledge
         generate_blocks(28800 * 7);

         for (const auto&p : mining_map)
         {
            auto mining_obj = db.find_pledge_mining(u_1000_id, u_3001_id);
            BOOST_CHECK(mining_obj == nullptr);
         }
      }



      {
         ACTORS((1000)(2000)(3001));

         const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
         auto _core = [&](int64_t x) -> asset
         {  return asset(x*prec);    };
         transfer(committee_account, u_1000_id, _core(600000));
         transfer(committee_account, u_2000_id, _core(600000));

         transfer(committee_account, u_3001_id, _core(600000));

         add_csaf_for_account(u_1000_id, 10000);
         add_csaf_for_account(u_2000_id, 10000);

         graphene::chain::pledge_mining::ext ext;
         ext.can_pledge = true;
         ext.bonus_rate = 6000;
         create_witness({ u_1000_private_key }, u_1000_id, "test pledge witness-1", 500000 * prec, u_1000_public_key, ext);
         create_witness({ u_2000_private_key }, u_2000_id, "test pledge witness-2", 500000 * prec, u_2000_public_key, ext);

         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_1000_id, 200000 * prec.value);
         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_2000_id, 100000 * prec.value);

         generate_blocks(9000);

         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_1000_id, 0);
         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_2000_id, 0);

         auto wit1 = db.get_witness_by_uid(u_1000_id);
         auto wit2 = db.get_witness_by_uid(u_2000_id);

         const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();
         share_type pledge_bonus1 = ((fc::bigint)(dpo.by_pledge_witness_pay_per_block.value) * 6000 * 200000 * prec.value
            / (700000 * prec.value * GRAPHENE_100_PERCENT)).to_int64();
         share_type pledge_bonus2 = ((fc::bigint)(dpo.by_pledge_witness_pay_per_block.value) * 6000 * 100000 * prec.value
            / (600000 * prec.value * GRAPHENE_100_PERCENT)).to_int64();

         auto account = db.get_account_statistics_by_uid(u_3001_id);
         auto bonus1 = wit1.total_produced * pledge_bonus1;
         auto bonus2 = wit2.total_produced * pledge_bonus2;
         BOOST_CHECK(account.uncollected_pledge_bonus == bonus1 + bonus2);

         auto witness_act1 = db.get_account_statistics_by_uid(u_1000_id);
         auto witness_act2 = db.get_account_statistics_by_uid(u_2000_id);
         auto wit_pay1 = dpo.by_pledge_witness_pay_per_block.value * wit1.total_produced - bonus1;
         auto wit_pay2 = dpo.by_pledge_witness_pay_per_block.value * wit2.total_produced - bonus2;
         BOOST_CHECK(witness_act1.uncollected_witness_pay == wit_pay1);
         BOOST_CHECK(witness_act2.uncollected_witness_pay == wit_pay2);
      }


      {
         ACTORS((1000)(2000)(3001));

         const share_type prec = asset::scaled_precision(asset_id_type()(db).precision);
         auto _core = [&](int64_t x) -> asset
         {  return asset(x*prec);    };
         transfer(committee_account, u_1000_id, _core(600000));
         transfer(committee_account, u_2000_id, _core(600000));

         transfer(committee_account, u_3001_id, _core(600000));

         add_csaf_for_account(u_1000_id, 10000);
         add_csaf_for_account(u_2000_id, 10000);

         graphene::chain::pledge_mining::ext ext;
         ext.can_pledge = true;
         ext.bonus_rate = 6000;
         create_witness({ u_1000_private_key }, u_1000_id, "test pledge witness-1", 500000 * prec, u_1000_public_key, ext);
         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_1000_id, 200000 * prec.value);
         generate_blocks(5000);

         update_mining_pledge({ u_3001_private_key }, u_3001_id, u_1000_id, 100000 * prec.value);

         const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();
         auto last_witness_pay = dpo.by_pledge_witness_pay_per_block;
         share_type pledge_bonus1 = ((fc::bigint)(dpo.by_pledge_witness_pay_per_block.value) * 6000 * 200000 * prec.value
            / (700000 * prec.value * GRAPHENE_100_PERCENT)).to_int64();
         auto wit1 = db.get_witness_by_uid(u_1000_id);
         auto bonus1 = wit1.total_produced * pledge_bonus1;
         auto account1 = db.get_account_statistics_by_uid(u_3001_id);
         BOOST_CHECK(account1.uncollected_pledge_bonus == bonus1);

         auto wit_pay1 = dpo.by_pledge_witness_pay_per_block.value * wit1.total_produced - bonus1;
         auto witness_act1 = db.get_account_statistics_by_uid(u_1000_id);
         BOOST_CHECK(witness_act1.uncollected_witness_pay == wit_pay1);

         auto last_pledge_witness_pay = dpo.by_pledge_witness_pay_per_block;
         auto last_produce_block = wit1.total_produced;
         bool is_produce_block = false;
         for (int i = 0; i < 20; ++i)
         {
            generate_block();
            const dynamic_global_property_object& dpo = db.get_dynamic_global_properties();
            if (last_pledge_witness_pay != dpo.by_pledge_witness_pay_per_block)
            {
               auto witness = db.get_witness_by_uid(u_1000_id);
               if (last_produce_block != db.get_witness_by_uid(u_1000_id).total_produced)
                  is_produce_block = true;
               break;
            }
         }

         auto block_num = db.get_witness_by_uid(u_1000_id).last_update_bonus_block_num + 10000 - db.head_block_num();
         generate_blocks(block_num);

         auto wit2 = db.get_witness_by_uid(u_1000_id);
         const dynamic_global_property_object& dpo2 = db.get_dynamic_global_properties();
         share_type pledge_bonus2 = ((fc::bigint)(dpo2.by_pledge_witness_pay_per_block.value) * 6000 * 100000 * prec.value
            / (600000 * prec.value * GRAPHENE_100_PERCENT)).to_int64();
         share_type bonus_per_pledge = 0;
         if (is_produce_block)
         {
            share_type bonus = ((fc::bigint)(last_witness_pay.value) * 6000 * 100000 * prec.value
               / (600000 * prec.value * GRAPHENE_100_PERCENT)).to_int64();
            auto total_bonus = (wit2.total_produced - last_produce_block - 1) * pledge_bonus2 + bonus;
            bonus_per_pledge = ((fc::uint128_t)total_bonus.value * GRAPHENE_PLEDGE_BONUS_PRECISION
               / wit2.total_mining_pledge).to_uint64();
         }
         else
            bonus_per_pledge = ((fc::uint128_t)((wit2.total_produced - last_produce_block) * pledge_bonus2).value * GRAPHENE_PLEDGE_BONUS_PRECISION
            / wit2.total_mining_pledge).to_uint64();

         share_type bonus2 = ((fc::uint128_t)bonus_per_pledge.value * 100000 * prec.value
            / GRAPHENE_PLEDGE_BONUS_PRECISION).to_uint64();

         auto account2 = db.get_account_statistics_by_uid(u_3001_id);
         auto account_uncollect_bonus = bonus1 + bonus2;
         BOOST_CHECK(account2.uncollected_pledge_bonus == account_uncollect_bonus);

      }
   }
   catch (fc::exception& e) {
      edump((e.to_detail_string()));
      throw;
   }
}

BOOST_AUTO_TEST_SUITE_END()
