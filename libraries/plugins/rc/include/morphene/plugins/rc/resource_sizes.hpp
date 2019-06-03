#pragma once

#include <fc/reflect/reflect.hpp>

// This header defines the resource sizes.  It is an implementation detail of resource_count.cpp,
// but you may want it if you are re-implementing resource_count.cpp in a client library.

#define STATE_BYTES_SCALE                       10000

// These numbers are obtained from computations in state_byte_hours.py script
//
// $ ./state_byte_hours.py
// {"seconds":196608}
// 174
// {"days":3}
// 229
// {"days":28}
// 1940
// {"days":7}
// 525

#define STATE_TRANSACTION_BYTE_SIZE               174
#define STATE_LIMIT_ORDER_BYTE_SIZE              1940

namespace morphene { namespace plugins { namespace rc {

struct state_object_size_info
{
   // authority
   int64_t authority_base_size                =   4    *STATE_BYTES_SCALE;
   int64_t authority_account_member_size      =  18    *STATE_BYTES_SCALE;
   int64_t authority_key_member_size          =  35    *STATE_BYTES_SCALE;

   // account_object
   int64_t account_object_base_size           = 480    *STATE_BYTES_SCALE;
   int64_t account_authority_object_base_size =  40    *STATE_BYTES_SCALE;

   // account_recovery_request_object
   int64_t account_recovery_request_object_base_size = 32*STATE_BYTES_SCALE;

   // transaction_object
   int64_t transaction_object_base_size       =  35    *STATE_TRANSACTION_BYTE_SIZE;
   int64_t transaction_object_byte_size       =         STATE_TRANSACTION_BYTE_SIZE;

   // vesting_delegation_object
   int64_t vesting_delegation_object_base_size = 60*STATE_BYTES_SCALE;

   // vesting_delegation_expiration_object
   int64_t vesting_delegation_expiration_object_base_size = 44*STATE_BYTES_SCALE;

   // withdraw_vesting_route_object
   int64_t withdraw_vesting_route_object_base_size = 43*STATE_BYTES_SCALE;

   // witness_object
   int64_t witness_object_base_size           = 266    *STATE_BYTES_SCALE;
   int64_t witness_object_url_char_size       =   1    *STATE_BYTES_SCALE;

   // witness_vote_object
   int64_t witness_vote_object_base_size      = 40     *STATE_BYTES_SCALE;
};

struct operation_exec_info
{
   int64_t account_create_operation_exec_time                  =  57700;
   int64_t account_create_with_delegation_operation_exec_time  =  57700;
   int64_t account_update_operation_exec_time                  =  14000;
   int64_t account_witness_proxy_operation_exec_time           = 117000;
   int64_t account_witness_vote_operation_exec_time            =  23000;
   int64_t change_recovery_account_operation_exec_time         =  12000;
   int64_t claim_account_operation_exec_time                   =  10000;
   int64_t create_claimed_account_operation_exec_time          =  57700;
   int64_t delegate_vesting_shares_operation_exec_time         =  19900;
   int64_t request_account_recovery_operation_exec_time        =  54400;
   int64_t set_withdraw_vesting_route_operation_exec_time      =  17900;
   int64_t transfer_operation_exec_time                        =   9600;
   int64_t transfer_to_vesting_operation_exec_time             =  44400;
   int64_t withdraw_vesting_operation_exec_time                =  10400;
   int64_t witness_set_properties_operation_exec_time          =   9500;
   int64_t witness_update_operation_exec_time                  =   9500;
};

} } }

FC_REFLECT( morphene::plugins::rc::state_object_size_info,
   ( authority_base_size )
   ( authority_account_member_size )
   ( authority_key_member_size )
   ( account_object_base_size )
   ( account_authority_object_base_size )
   ( account_recovery_request_object_base_size )
   ( transaction_object_base_size )
   ( transaction_object_byte_size )
   ( vesting_delegation_object_base_size )
   ( vesting_delegation_expiration_object_base_size )
   ( withdraw_vesting_route_object_base_size )
   ( witness_object_base_size )
   ( witness_object_url_char_size )
   ( witness_vote_object_base_size )
   )

FC_REFLECT( morphene::plugins::rc::operation_exec_info,
   ( account_create_operation_exec_time )
   ( account_create_with_delegation_operation_exec_time )
   ( account_update_operation_exec_time )
   ( account_witness_proxy_operation_exec_time )
   ( account_witness_vote_operation_exec_time )
   ( change_recovery_account_operation_exec_time )
   ( claim_account_operation_exec_time )
   ( create_claimed_account_operation_exec_time )
   ( delegate_vesting_shares_operation_exec_time )
   ( request_account_recovery_operation_exec_time )
   ( set_withdraw_vesting_route_operation_exec_time )
   ( transfer_operation_exec_time )
   ( transfer_to_vesting_operation_exec_time )
   ( withdraw_vesting_operation_exec_time )
   ( witness_set_properties_operation_exec_time )
   ( witness_update_operation_exec_time ) )
