#pragma once
#include <fc/variant.hpp>
#include <fc/optional.hpp>
#include <fc/api.hpp>
#include <fc/any.hpp>
#include <memory>
#include <vector>
#include <functional>
#include <utility>
#include <fc/signals.hpp>
//#include <fc/rpc/json_connection.hpp>

namespace fc {
   class api_connection;
   
   typedef uint32_t api_id_type;

   namespace detail {
      template<typename Signature>
      class callback_functor
      {
         public:
            typedef typename std::function<Signature>::result_type result_type;

            callback_functor( fc::api_connection& con, uint64_t id )
            :_callback_id(id),_api_connection(con){}

            template<typename... Args> 
            result_type operator()( Args... args )const;

         private:
            uint64_t                           _callback_id;
            fc::api_connection&                _api_connection;
      };

      template<typename R, typename Arg0, typename ... Args>
      std::function<R(Args...)> bind_first_arg( const std::function<R(Arg0,Args...)>& f, Arg0 a0 )
      {
         return [=]( Args... args ) { return f( a0, args... ); };
      }
      template<typename R>
      R call_generic( const std::function<R()>& f, variants::const_iterator a0, variants::const_iterator e )
      {
         return f();
      }

      template<typename R, typename Arg0, typename ... Args>
      R call_generic( const std::function<R(Arg0,Args...)>& f, variants::const_iterator a0, variants::const_iterator e )
      {
         FC_ASSERT( a0 != e );
         return  call_generic<R,Args...>( bind_first_arg<R,Arg0,Args...>( f, a0->as< typename std::decay<Arg0>::type >() ), a0+1, e );
      }

      template<typename R, typename ... Args>
      std::function<variant(const fc::variants&)> to_generic( const std::function<R(Args...)>& f )
      {
         return [=]( const variants& args ) { 
            return variant( call_generic( f, args.begin(), args.end() ) ); 
         };
      }

      template<typename ... Args>
      std::function<variant(const fc::variants&)> to_generic( const std::function<void(Args...)>& f )
      {
         return [=]( const variants& args ) { 
            call_generic( f, args.begin(), args.end() ); 
            return variant();
         };
      }

   } // namespace detail

   class generic_api
   {
      public:
         template<typename Api>
         generic_api( const Api& a, const std::shared_ptr<fc::api_connection>& c );

         generic_api( const generic_api& cpy ) = delete;

         variant call( const string& name, const variants& args )
         {
            auto itr = _by_name.find(name);
            FC_ASSERT( itr != _by_name.end(), "no method with name '${name}'", ("name",name)("api",_by_name) );
            return call( itr->second, args );
         }

         variant call( uint32_t method_id, const variants& args )
         {
            FC_ASSERT( method_id < _methods.size() );
            return _methods[method_id](args);
         }

         fc::api_connection&  get_connection(){ auto tmp = _api_connection.lock(); FC_ASSERT( tmp, "connection closed"); return *tmp; }


      private:
         friend struct api_visitor;

         template<typename R, typename Arg0, typename ... Args>
         std::function<R(Args...)> bind_first_arg( const std::function<R(Arg0,Args...)>& f, Arg0 a0 )const
         {
            return [=]( Args... args ) { return f( a0, args... ); };
         }

         template<typename R>
         R call_generic( const std::function<R()>& f, variants::const_iterator a0, variants::const_iterator e )const
         {
            return f();
         }

         template<typename R, typename Signature, typename ... Args>
         R call_generic( const std::function<R(std::function<Signature>,Args...)>& f, variants::const_iterator a0, variants::const_iterator e )
         {
            FC_ASSERT( a0 != e, "too few arguments passed to method" );
            detail::callback_functor<Signature> arg0( get_connection(), a0->as<uint64_t>() );
            return  call_generic<R,Args...>( this->bind_first_arg<R,std::function<Signature>,Args...>( f, std::function<Signature>(arg0) ), a0+1, e );
         }
         template<typename R, typename Signature, typename ... Args>
         R call_generic( const std::function<R(const std::function<Signature>&,Args...)>& f, variants::const_iterator a0, variants::const_iterator e )
         {
            FC_ASSERT( a0 != e, "too few arguments passed to method" );
            detail::callback_functor<Signature> arg0( get_connection(), a0->as<uint64_t>() );
            return  call_generic<R,Args...>( this->bind_first_arg<R,const std::function<Signature>&,Args...>( f, arg0 ), a0+1, e );
         }

         template<typename R, typename Arg0, typename ... Args>
         R call_generic( const std::function<R(Arg0,Args...)>& f, variants::const_iterator a0, variants::const_iterator e )
         {
            FC_ASSERT( a0 != e, "too few arguments passed to method" );
            return  call_generic<R,Args...>( this->bind_first_arg<R,Arg0,Args...>( f, a0->as< typename std::decay<Arg0>::type >() ), a0+1, e );
         }

         struct api_visitor
         {
            api_visitor( generic_api& a, const std::weak_ptr<fc::api_connection>& s ):api(a),_api_con(s){ }

            template<typename Interface, typename Adaptor, typename ... Args>
            std::function<variant(const fc::variants&)> to_generic( const std::function<api<Interface,Adaptor>(Args...)>& f )const;

            template<typename Interface, typename Adaptor, typename ... Args>
            std::function<variant(const fc::variants&)> to_generic( const std::function<fc::optional<api<Interface,Adaptor>>(Args...)>& f )const;

            template<typename R, typename ... Args>
            std::function<variant(const fc::variants&)> to_generic( const std::function<R(Args...)>& f )const;

            template<typename ... Args>
            std::function<variant(const fc::variants&)> to_generic( const std::function<void(Args...)>& f )const;

            template<typename Result, typename... Args>
            void operator()( const char* name, std::function<Result(Args...)>& memb )const {
               api._methods.emplace_back( to_generic( memb ) ); 
               api._by_name[name] = api._methods.size() - 1;
            }

            generic_api&  api;
            const std::weak_ptr<fc::api_connection>& _api_con;
         };


         std::weak_ptr<fc::api_connection>                       _api_connection;
         fc::any                                                 _api;
         std::map< std::string, uint32_t >                       _by_name;
         std::vector< std::function<variant(const variants&)> >  _methods;
   }; // class generic_api



   class api_connection : public std::enable_shared_from_this<fc::api_connection>
   {
      public:
         api_connection(){}
         virtual ~api_connection(){};


         template<typename T>
         api<T> get_remote_api( api_id_type api_id = 0 )
         {
            api<T> result;
            result->visit( api_visitor(  api_id, this->shared_from_this() ) );
            return result;
         }
  
         /** makes calls to the remote server */
         virtual variant send_call( api_id_type api_id, string method_name, variants args = variants() ) = 0;
         virtual variant send_callback( uint64_t callback_id, variants args = variants() ) = 0;
         virtual void    send_notice( uint64_t callback_id, variants args = variants() ) = 0;

         variant receive_call( api_id_type api_id, const string& method_name, const variants& args = variants() )const
         {
            FC_ASSERT( _local_apis.size() > api_id );
            return _local_apis[api_id]->call( method_name, args );
         }
         variant receive_callback( uint64_t callback_id,  const variants& args = variants() )const
         {
            FC_ASSERT( _local_callbacks.size() > callback_id );
            return _local_callbacks[callback_id]( args );
         }
         void receive_notice( uint64_t callback_id,  const variants& args = variants() )const
         {
            FC_ASSERT( _local_callbacks.size() > callback_id );
            _local_callbacks[callback_id]( args );
         }

         template<typename Interface>
         api_id_type register_api( const Interface& a )
         {
            auto handle = a.get_handle();
            auto itr = _handle_to_id.find(handle);
            if( itr != _handle_to_id.end() ) return itr->second;

            _local_apis.push_back( std::unique_ptr<generic_api>( new generic_api(a, shared_from_this() ) ) );
            _handle_to_id[handle] = _local_apis.size() - 1;
            return _local_apis.size() - 1;
         }

         template<typename Signature>
         uint64_t register_callback( const std::function<Signature>& cb )
         {
            _local_callbacks.push_back( detail::to_generic( cb ) );
            return _local_callbacks.size() - 1;
         }

         fc::signal<void()> closed;
      private:
         std::vector< std::unique_ptr<generic_api> >     _local_apis;
         std::map< uint64_t, api_id_type >                       _handle_to_id;
         std::vector< std::function<variant(const variants&)>  > _local_callbacks;


         struct api_visitor
         {
            uint32_t                            _api_id;
            std::shared_ptr<fc::api_connection> _connection;

            api_visitor( uint32_t api_id, std::shared_ptr<fc::api_connection> con )
            :_api_id(api_id),_connection(std::move(con))
            {
            }

            api_visitor() = delete;

            template<typename Result>
            static Result from_variant( const variant& v, Result*, const std::shared_ptr<fc::api_connection>&  )
            {
               return v.as<Result>();
            }

            template<typename ResultInterface>
            static fc::api<ResultInterface> from_variant( const variant& v, 
                                                          fc::api<ResultInterface>* /*used for template deduction*/,
                                                          const std::shared_ptr<fc::api_connection>&  con 
                                                        )
            {
               return con->get_remote_api<ResultInterface>( v.as_uint64() );
            }

            template<typename T>
            static fc::variant convert_callbacks( const std::shared_ptr<fc::api_connection>&, const T& v ) 
            { 
               return fc::variant(v); 
            }

            template<typename Signature>
            static fc::variant convert_callbacks( const std::shared_ptr<fc::api_connection>& con, const std::function<Signature>& v ) 
            { 
               return con->register_callback( v ); 
            }

            template<typename Result, typename... Args>
            void operator()( const char* name, std::function<Result(Args...)>& memb )const 
            {
                auto con   = _connection;
                auto api_id = _api_id;
                memb = [con,api_id,name]( Args... args ) {
                    auto var_result = con->send_call( api_id, name, { convert_callbacks(con,args)...} );
                    return from_variant( var_result, (Result*)nullptr, con );
                };
            }
            template<typename... Args>
            void operator()( const char* name, std::function<void(Args...)>& memb )const 
            {
                auto con   = _connection;
                auto api_id = _api_id;
                memb = [con,api_id,name]( Args... args ) {
                   con->send_call( api_id, name, { convert_callbacks(con,args)...} );
                };
            }
         };
   };

   class local_api_connection : public api_connection
   {
      public:
         /** makes calls to the remote server */
         virtual variant send_call( api_id_type api_id, string method_name, variants args = variants() ) override
         {
            FC_ASSERT( _remote_connection );
            return _remote_connection->receive_call( api_id, method_name, std::move(args) );
         }
         virtual variant send_callback( uint64_t callback_id, variants args = variants() ) override
         {
            FC_ASSERT( _remote_connection );
            return _remote_connection->receive_callback( callback_id, args );
         }
         virtual void send_notice( uint64_t callback_id, variants args = variants() ) override
         {
            FC_ASSERT( _remote_connection );
            _remote_connection->receive_notice( callback_id, args );
         }


         void  set_remote_connection( const std::shared_ptr<fc::api_connection>& rc )
         {
            FC_ASSERT( !_remote_connection );
            FC_ASSERT( rc != this->shared_from_this() );
            _remote_connection = rc;
         }
         const std::shared_ptr<fc::api_connection>& remote_connection()const  { return _remote_connection; }

         std::shared_ptr<fc::api_connection>    _remote_connection;
   };

   template<typename Api>
   generic_api::generic_api( const Api& a, const std::shared_ptr<fc::api_connection>& c )
   :_api_connection(c),_api(a)
   {
      boost::any_cast<const Api&>(a)->visit( api_visitor( *this, c ) );
   }

   template<typename Interface, typename Adaptor, typename ... Args>
   std::function<variant(const fc::variants&)> generic_api::api_visitor::to_generic( 
                                               const std::function<fc::api<Interface,Adaptor>(Args...)>& f )const
   {
      auto api_con = _api_con;
      auto gapi = &api;
      return [=]( const variants& args ) { 
         auto con = api_con.lock();
         FC_ASSERT( con, "not connected" );

         auto api_result = gapi->call_generic( f, args.begin(), args.end() ); 
         return con->register_api( api_result );
      };
   }
   template<typename Interface, typename Adaptor, typename ... Args>
   std::function<variant(const fc::variants&)> generic_api::api_visitor::to_generic( 
                                               const std::function<fc::optional<fc::api<Interface,Adaptor>>(Args...)>& f )const
   {
      auto api_con = _api_con;
      auto gapi = &api;
      return [=]( const variants& args )-> fc::variant { 
         auto con = api_con.lock();
         FC_ASSERT( con, "not connected" );

         auto api_result = gapi->call_generic( f, args.begin(), args.end() ); 
         if( api_result )
            return con->register_api( *api_result );
         return variant();
      };
   }
   template<typename R, typename ... Args>
   std::function<variant(const fc::variants&)> generic_api::api_visitor::to_generic( const std::function<R(Args...)>& f )const
   {
      generic_api* gapi = &api;
      return [f,gapi]( const variants& args ) { 
         return variant( gapi->call_generic( f, args.begin(), args.end() ) ); 
      };
   }

   template<typename ... Args>
   std::function<variant(const fc::variants&)> generic_api::api_visitor::to_generic( const std::function<void(Args...)>& f )const
   {
      generic_api* gapi = &api;
      return [f,gapi]( const variants& args ) { 
         gapi->call_generic( f, args.begin(), args.end() ); 
         return variant();
      };
   }

   namespace detail {
      template<typename Signature>
      template<typename... Args> 
      typename callback_functor<Signature>::result_type callback_functor<Signature>::operator()( Args... args )const
      {
         _api_connection.send_callback( _callback_id, fc::variants{ args... } ).template as< result_type >();
      }


      template<typename... Args>
      class callback_functor<void(Args...)>
      {
         public:
          typedef void result_type;

          callback_functor( fc::api_connection& con, uint64_t id )
          :_callback_id(id),_api_connection(con){}

          void operator()( Args... args )const
          {
             _api_connection.send_notice( _callback_id, fc::variants{ args... } );
          }

         private:
          uint64_t            _callback_id;
          fc::api_connection& _api_connection;
      };
   } // namespace detail

} // fc
