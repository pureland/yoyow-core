#include "../yoyow_sign.h"
#include <iostream>
#include <stdio.h>

using namespace std;

int main()
{
   string tx = "{\"ref_block_num\": 0,\"ref_block_prefix\": 0,\"expiration\": \"1970-01-01T00:00:00\",\"operations\": [[0, {\"fee\": {\"total\": {\"amount\": 20000,\"asset_id\": 0},\"options\": {\"from_csaf\": {\"amount\": 20000,\"asset_id\": 0}}},\"from\": 25997,\"to\": 26264,\"amount\": {\"amount\": 100000,\"asset_id\": 0}}]],\"signatures\": []}";
   string wif = "5JuTEwvFqnhjWRtvdUhTxVDhZBG71QgiRTmRV1B5Ph4jFmWBM7F";
   string chain = "ae4f234c75199f67e526c9478cf499dd6e94c2b66830ee5c58d0868a3179baf6";

   string result = signature( tx, wif, chain );
   printf("signature:\n%s\n", result.c_str() );

   string pvkey = generate_key(chain);
   printf("generate_key:\n%s\n", pvkey.c_str());

   printf("private_to_public:\n%s\n", private_to_public(pvkey).c_str());

   string trx = generate_transaction("01d2df3dc73f6f04072c117ca4dcae1d46876f44",
                                     "2018-09-18T01:51:06",
                                     "263324063",
                                     "26264",
                                     "10",
                                     "memo",
                                     "5JPTHdCy5QNhgKsTeQGxPRxM5FYj9MKDLoNLvTcrKQgg5GqAPa7",
                                     "YYW6yyVSHhKGq9a3BZ99HMRLygbxUf5JHo3vChTBtGXtuvwzZRLnv",
                                     "{\"parameters\":[[0,{\"fee\":20000,\"price_per_kbyte\":100000,\"min_real_fee\":0,\"min_rf_percent\": 0}]]}");
   printf("generate_transaction:\n%s\n", trx.c_str());

   string memo = decrypt_memo( "{\"from\":\"YYW5q8zUko5Evds7dnh486afwavF7oW8aje7DeusGABVMTfcChFvn\",\"to\":\"YYW6nBpB5aCTL84bA34qVwaskRtoz7XaTW66LzLy3pKP7ta5mZVrs\",\"nonce\":\"6558778315539393896\",\"message\":\"0437fc73cd356964784a3b169b9b6db3ac642fa278719a82acad1a4f3ae8ce16\"}", 
                               "5JPTHdCy5QNhgKsTeQGxPRxM5FYj9MKDLoNLvTcrKQgg5GqAPa7");
   printf("decrypt_memo:\n%s\n", memo.c_str());

   // update_auth
   tx = "{\"ref_block_prefix\":4151460282,\"operations\":[[3,{\"owner\":{\"account_uid_auths\":[],\"key_auths\":[[\"YYW5qTjzqQM2wjxYwVEai32i74Vvq2UJzEXBpUKL3FR1VYH4T23MF\",1]],\"weight_threshold\":1},\"secondary\":{\"account_uid_auths\":[],\"key_auths\":[[\"YYW79gbH9egqK4TuWhgijsqsTCKprHZPNPBKxKhTDKGeUacoGVyZZ\",1]],\"weight_threshold\":1},\"uid\":\"438553867\",\"fee\":{\"total\":{\"amount\":289840,\"asset_id\":0},\"options\":{\"from_csaf\":{\"amount\":289840,\"asset_id\":0}}},\"active\":{\"account_uid_auths\":[],\"key_auths\":[[\"YYW58pYuWmxhDH3GQbC4jGdDb6x3FKqE8B1GmCGEK1aA72LhVGzY8\",1]],\"weight_threshold\":1}}]],\"expiration\":\"2018-09-26T10:53:42\",\"ref_block_num\":55404,\"signatures\":[]}";
   result = signature( tx, wif, chain );
   printf("signature[update_auth]:\n%s\n", result.c_str() );

   return 0;
}