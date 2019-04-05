#include <morphene/protocol/types_fwd.hpp>

#include <morphene/schema/schema.hpp>
#include <morphene/schema/schema_impl.hpp>
#include <morphene/schema/schema_types.hpp>

#include <morphene/chain/schema_types/oid.hpp>
#include <morphene/protocol/schema_types/account_name_type.hpp>
#include <morphene/protocol/schema_types/asset_symbol_type.hpp>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <morphene/chain/account_object.hpp>
#include <morphene/chain/morphene_objects.hpp>
#include <morphene/chain/database.hpp>
#include <morphene/chain/index.hpp>

using morphene::schema::abstract_schema;

struct schema_info
{
   schema_info( std::shared_ptr< abstract_schema > s )
   {
      std::vector< std::shared_ptr< abstract_schema > > dep_schemas;
      s->get_deps( dep_schemas );
      for( const std::shared_ptr< abstract_schema >& ds : dep_schemas )
      {
         deps.emplace_back();
         ds->get_name( deps.back() );
      }
      std::string str_schema;
      s->get_str_schema( str_schema );
      schema = fc::json::from_string( str_schema );
   }

   std::vector< std::string >   deps;
   fc::variant                  schema;
};

void add_to_schema_map(
   std::map< std::string, schema_info >& m,
   std::shared_ptr< abstract_schema > schema,
   bool follow_deps = true )
{
   std::string name;
   schema->get_name( name );

   if( m.find( name ) != m.end() )
      return;
   // TODO:  Use iterative, not recursive, algorithm
   m.emplace( name, schema );

   if( !follow_deps )
      return;

   std::vector< std::shared_ptr< abstract_schema > > dep_schemas;
   schema->get_deps( dep_schemas );
   for( const std::shared_ptr< abstract_schema >& ds : dep_schemas )
      add_to_schema_map( m, ds, follow_deps );
}

struct morphene_schema
{
   std::map< std::string, schema_info >     schema_map;
   std::vector< std::string >               chain_object_types;
};

FC_REFLECT( schema_info, (deps)(schema) )
FC_REFLECT( morphene_schema, (schema_map)(chain_object_types) )

int main( int argc, char** argv, char** envp )
{
   morphene::chain::database db;
   morphene::chain::database::open_args db_args;

   db_args.data_dir = "tempdata";
   db_args.shared_mem_dir = "tempdata/blockchain";
   db_args.shared_file_size = 1024*1024*8;

   std::map< std::string, schema_info > schema_map;

   db.open( db_args );

   morphene_schema ss;

   std::vector< std::string > chain_objects;
   db.for_each_index_extension< morphene::chain::index_info >(
      [&]( std::shared_ptr< morphene::chain::index_info > info )
      {
         std::string name;
         info->get_schema()->get_name( name );
         // std::cout << name << std::endl;

         add_to_schema_map( ss.schema_map, info->get_schema() );
         ss.chain_object_types.push_back( name );
      } );

   std::cout << fc::json::to_string( ss ) << std::endl;

   db.close();

   return 0;
}
