//proxy_tcp_svr_test.cpp

#include <iostream>

#include <yggr/base/exception.hpp>
#include <yggr/base/ctrl_center.hpp>
#include <yggr/base/system_code.hpp>

#include <yggr/ids/uuid.hpp>
#include <yggr/ids/base_ids_def.hpp>

#include <yggr/encryption_tool/blowfish_tool.hpp>
#include <yggr/encryption_tool/md5_tool.hpp>
#include <yggr/encryption_tool/safe_packet_tool.hpp>
#include <yggr/encryption_tool/packet_cypher_tool.hpp>

#include <yggr/archive/net_oarchive.hpp>
#include <yggr/archive/net_iarchive.hpp>
#include <yggr/archive/network_archive_partner.hpp>

#include <yggr/task_center/task.hpp>
#include <yggr/task_center/task_creator.hpp>
#include <yggr/task_center/task_center.hpp>
#include <yggr/task_center/work_runner_saver.hpp>
#include <yggr/task_center/recver_handler_mgr.hpp>

#include <yggr/safe_container/safe_hash_map_queue.hpp>

#include <yggr/adapter/adapter_mgr.hpp>
#include <yggr/adapter/adapter_parser_helper.hpp>

#include <yggr/thread/static_work_runner.hpp> // use static_work_runner
#include <yggr/thread/thread_mgr.hpp>
#include <yggr/thread/boost_thread_config.hpp>

#include <yggr/network/network_config/network_balance_tcp_config.hpp>
//#include <yggr/network/start_data/pak_back_id.hpp>
#include <yggr/network/start_data/encryption_pak_back_id.hpp>
#include <yggr/network/session_helper/session_crypher_creator.hpp>
#include <yggr/network/session_helper/session_checker_creator.hpp>
#include <yggr/network/session_helper/first_action.hpp>
#include <yggr/network/session_helper/owner_generator.hpp>
#include <yggr/network/session_helper/session_creator.hpp>

#include <yggr/network/packets_support/packets_checker.hpp>
#include <yggr/network/packets_support/packets_crypher.hpp>

#include <yggr/network/adapter_helper/io_converter.hpp>
#include <yggr/network/support/data_info_def.hpp>
#include <yggr/network/network_info.hpp>
#include <yggr/network/network_packet.hpp>
#include <yggr/network/network_config/network_tcp_config.hpp>
#include <yggr/network/connection.hpp>
#include <yggr/network/ex_linker.hpp>

#include <yggr/network/session.hpp>
#include <yggr/network/session_mgr.hpp>
#include <yggr/network/session_helper/session_tcp_curer.hpp>
#include <yggr/network/service_handler.hpp>
#include <yggr/network/network_handler.hpp>

#include <yggr/network/session_state/session_state_fixer.hpp>
#include <yggr/network/session_state/session_state_checker.hpp>

#include <yggr/proxy/proxy_msg/proxy_register_msg.hpp>
#include <yggr/proxy/proxy_msg/proxy_register_back_msg.hpp>
#include <yggr/proxy/proxy_msg/proxy_mode_change_back_msg.hpp>
#include <yggr/proxy/proxy_msg/proxy_mode_change_msg.hpp>
#include <yggr/proxy/proxy_msg/proxy_unregister_msg.hpp>
#include <yggr/proxy/proxy_msg/proxy_unregister_back_msg.hpp>

#include <yggr/proxy/proxy_mode/proxy_mode_def.hpp>
#include <yggr/proxy/proxy_fix_state_def.hpp>

#include <yggr/client/client_config/client_tcp_config.hpp>
#include <yggr/client/start_mode/client_passive_tcp_start_mode.hpp>
#include <yggr/client/basic_clt_handler.hpp>
#include <yggr/client/client.hpp>


#include <yggr/log/yggr_exception_log_accesser.hpp>

#include <boost/enable_shared_from_this.hpp>
#include <boost/unordered_set.hpp>

#include <cassert>

#ifdef _MSC_VER
#	include <vld.h>
#endif //_MSC_VER


//----------------------------------------------------------------------------------
typedef yggr::encryption_tool::blowfish_tool<> blowfish_tool_type;
typedef yggr::encryption_tool::md5_tool md5_tool_type;
typedef yggr::encryption_tool::safe_packet_tool<blowfish_tool_type, md5_tool_type> safe_packet_tool_type;
typedef yggr::encryption_tool::packet_cypher_tool<safe_packet_tool_type> packet_cypher_tool_type;

typedef yggr::system_controller::ctrl_center ctrl_center_type;

//typedef yggr::network::start_data::pak_back_id<yggr::ids::id64_type> test_pak_type;
typedef yggr::network::start_data::encryption_pak_back_id<yggr::ids::id64_type, std::string, 8> test_pak_type;

typedef yggr::network::network_info<yggr::ids::id64_type> network_info_type;
typedef yggr::network::support::network_data_info_def<yggr::u32, yggr::u16> data_info_def_type;

typedef yggr::network::network_packet<
				yggr::archive::archive_partner::network_oarchive_partner,
				data_info_def_type,
				network_info_type
			> net_opak_type;

typedef yggr::network::network_packet<
				yggr::archive::archive_partner::network_iarchive_partner,
				data_info_def_type,
				network_info_type
			> net_ipak_type;

typedef yggr::ids::id_generator<yggr::ids::uuid,
								yggr::ids::parse_uuid_genner::lagged_fibonacci607_type> task_id_gen_type;

typedef yggr::task_center::task<yggr::ids::uuid, network_info_type,
									yggr::task_center::default_task_data_info_type> task_type;

typedef yggr::task_center::support::task_data_info_parser<yggr::task_center::default_task_data_info_type> task_data_info_parser_type;

YGGR_PP_THREAD_ACTRION_HELPER_MAKE_REG_ID_PARSER(action_reg_string_parser,
													std::string,
													task_data_info_parser_type,
													class_name)

typedef yggr::thread::action_helper::action_reg_string_parser action_reg_id_parser_type;

typedef yggr::thread::action_helper::action_id_parser<action_reg_id_parser_type,
														task_type::class_name_getter> action_id_parser_type;

typedef yggr::thread::static_work_runner<task_type, action_id_parser_type> work_runner_type;

typedef work_runner_type::shared_info_ptr_type runner_shared_info_ptr_type;

typedef yggr::task_center::work_runner_saver<yggr::task_center::type_traits::mark_saver_condition,
												work_runner_type> cdt_saver_type;


typedef yggr::task_center::recver_handler_mgr<yggr::task_center::type_traits::mark_saver_result, task_type> rst_saver_type;

typedef yggr::task_center::task_creator<task_id_gen_type, task_type> task_creator_type;

typedef yggr::task_center::task_center<task_creator_type, cdt_saver_type, rst_saver_type> task_center_type;

typedef yggr::adapter::adaper_id_parser_def
		<
			//Reg_Send_ID_Parser
			yggr::task_center::task_data_send_typeid_getter
			<
				yggr::task_center::default_task_data_info_type
			>,

			//Reg_Recv_ID_Parser
			yggr::task_center::task_data_recv_typeid_getter
			<
				yggr::task_center::default_task_data_info_type
			>,

			//Send_ID_Parser
			YGGR_IDS_PARSER_CONST_MEM_FOO(net_ipak_type::base_type,
											const yggr
													::task_center
														::default_task_data_info_type
															::data_info_type&,
											data_info),

			//Task_Send_ID_Parser
			YGGR_IDS_PARSER_CONST_MEM_FOO(task_type,
											const yggr
												::task_center
													::default_task_data_info_type
														::data_info_type&,
											data_info),

			//Task_Recv_ID_Parser
			YGGR_IDS_PARSER_CONST_MEM_FOO(task_type,
											yggr
												::task_center
													::default_task_data_info_type
														::class_name_type,
											class_name),

			//Recv_ID_Parser
			YGGR_IDS_PARSER_USE_TYPEID_NAME_CONV_RET(std::string)
		> adaper_id_parser_def_type;


typedef yggr::adapter::adapter_mgr<task_center_type,
									net_ipak_type,
									net_opak_type,
									adaper_id_parser_def_type,
									yggr::network::adapter_helper::io_converter> adapter_mgr_type;

typedef adapter_mgr_type::reg_def_type adapter_mgr_reg_def_type;

typedef yggr::network::ex_linker<yggr::network::network_config::balance_tcpv4_config> linker_type;

typedef yggr::network::connection<linker_type, net_opak_type, net_ipak_type> conn_type;

//typedef yggr::network::packets_support::packets_checker<network_info_type> packets_checker_type;
//YGGR_PP_REGISTER_SESSION_CHECKER_CREATOR_BEGIN(test_pak_type, test_pak, packets_checker_type)
//	test_pak.id()
//YGGR_PP_REGISTER_SESSION_CHECKER_CREATOR_END()
//
//typedef yggr::network::packets_support::packets_crypher<void> packets_crypher_type;
//YGGR_PP_REGISTER_SESSION_CRYPHER_CREATOR_BEGIN(test_pak_type, test_pak, packets_crypher_type)
//YGGR_PP_REGISTER_SESSION_CRYPHER_CREATOR_END()

//encryption def
typedef yggr::network::packets_support::packets_checker<network_info_type, 1000> packets_checker_type;
YGGR_PP_REGISTER_SESSION_CHECKER_CREATOR_BEGIN(test_pak_type, test_pak, packets_checker_type)
	test_pak.id()
YGGR_PP_REGISTER_SESSION_CHECKER_CREATOR_END()

typedef yggr::network::packets_support::packets_crypher<packet_cypher_tool_type> packets_crypher_type;
YGGR_PP_REGISTER_SESSION_CRYPHER_CREATOR_BEGIN(test_pak_type, test_pak, packets_crypher_type)
	test_pak.key()
YGGR_PP_REGISTER_SESSION_CRYPHER_CREATOR_END()

typedef yggr::network::session_helper::first_action first_action_type;

typedef yggr::network::session<yggr::network::type_traits::tag_client,
								conn_type,
								adapter_mgr_type,
								packets_checker_type,
								packets_crypher_type,
								first_action_type,
								yggr::network::session_state::default_session_state_fixer_type
							> session_type;

typedef yggr::network::start_data::start_data_loader<test_pak_type> start_data_loader_type;

typedef yggr::network::session_helper::session_creator<test_pak_type, conn_type,
														yggr::network::session_helper::session_checker_creator,
														yggr::network::session_helper::session_crypher_creator
														> session_creator_type;

typedef yggr::client::start_mode::client_passive_tcp_start_mode<
																net_ipak_type,
																start_data_loader_type,
																session_creator_type
															> client_start_mode;

typedef yggr::network::session_helper::session_tcp_curer<session_type> session_curer_type;

typedef yggr::network::session_mgr<session_type,
									client_start_mode,
									yggr::network::session_state::default_session_state_checker_type,
									session_curer_type
									> session_mgr_type;

typedef yggr::network::service_handler<	session_mgr_type::service_type,
										boost::asio::signal_set,
										yggr::thread::boost_thread_config_type,
										std::allocator,
										std::vector,
										yggr::network::balance_io_service_selector
									> service_handler_type;


typedef yggr::client::basic_clt_handler<yggr::client::client_config::client_tcp_config_type,
										service_handler_type,
										session_mgr_type
									> tcp_clt_handler_type;

typedef yggr::network::network_handler<tcp_clt_handler_type> network_handler_type;

typedef yggr::client::client<network_handler_type> clt_type;

typedef yggr::ptr_single<clt_type> clt_ptr_single_type;
typedef clt_ptr_single_type::obj_ptr_type clt_ptr_type;

//-------------------------------------------------------------------

static yggr::u32 send_count = 0;
static yggr::u32 now_state = 0;
static yggr::u32 chg_state = 0;

typedef yggr::proxy::proxy_msg::proxy_register_msg<> proxy_register_msg_type;
typedef yggr::proxy::proxy_msg::proxy_register_back_msg<> proxy_register_back_msg_type;
typedef yggr::proxy::proxy_msg::proxy_mode_change_msg<> proxy_mode_change_msg_type;
typedef yggr::proxy::proxy_msg::proxy_mode_change_back_msg<> proxy_mode_change_back_msg_type;
typedef yggr::proxy::proxy_msg::proxy_unregister_msg<> proxy_unregister_msg_type;
typedef yggr::proxy::proxy_msg::proxy_unregister_back_msg<> proxy_unregister_back_msg_type;

typedef yggr::proxy::proxy_mode::proxy_mode_def proxy_mode_def_type;
typedef yggr::proxy::proxy_fix_state_def proxy_fix_state_def_type;

struct Calculator : public boost::enable_shared_from_this<Calculator>
{
private:
	typedef Calculator this_type;
public:
	ERROR_MAKER_BEGIN("Clt_Calculator")
		ERROR_CODE_DEF_NON_CODE_BEGIN()
			ERROR_CODE_DEF(E_code_test)
		ERROR_CODE_DEF_NON_CODE_END()

		ERROR_CODE_MSG_BEGIN()
			ERROR_CODE_MSG(E_code_test, "code test")
		ERROR_CODE_MSG_END()
	ERROR_MAKER_END()

	typedef network_info_type owner_info_type;

	typedef boost::unordered_multiset<owner_info_type> owner_info_container_type;

	template<typename Runner, typename Action_Table, typename Recv_Handler>
	void register_cal_object(Action_Table& at, const Recv_Handler& handler)
	{
		typedef Runner runner_type;
		typedef Action_Table action_table_type;
		typedef Recv_Handler recv_handler_type;

		typedef Runner runner_type;
		typedef Action_Table action_table_type;
		typedef Recv_Handler recv_handler_type;

		at.template register_calculator<test_pak_type, Recv_Handler>(
				handler, boost::bind(&this_type::cal_test_pak_type<owner_info_type,
																	runner_type,
																	recv_handler_type>,
									shared_from_this(), _1, _2, _3, _4));

		at.template register_calculator<proxy_register_back_msg_type, Recv_Handler>(
				handler, boost::bind(&this_type::cal_register_back_msg_type<owner_info_type,
																				runner_type,
																				recv_handler_type>,
									shared_from_this(), _1, _2, _3, _4));

		at.template register_calculator<proxy_mode_change_back_msg_type, Recv_Handler>(
				handler, boost::bind(&this_type::cal_mode_change_back_msg_type<owner_info_type,
																				runner_type,
																				recv_handler_type>,
									shared_from_this(), _1, _2, _3, _4));

		at.template register_calculator<proxy_unregister_back_msg_type, Recv_Handler>(
				handler, boost::bind(&this_type::cal_proxy_unregister_back_msg_type<owner_info_type,
																				runner_type,
																				recv_handler_type>,
									shared_from_this(), _1, _2, _3, _4));
	}


	template<typename Owner, typename Runner, typename Handler>
	void cal_test_pak_type(const Owner& owner, const test_pak_type& cdt, Runner* prunner, const Handler& handler)
	{
		//static int count = 0;
		static int i = 0;

		if(owner.size() == 1)
		{
			proxy_register_msg_type msg;
			/*msg.add_reg_data(test_pak_type::s_data_info(),
								proxy_mode_def_type::E_proxy_mode_monopolize,
								test_pak_type::s_cal_type());*/

			msg.add_reg_data(test_pak_type::s_data_info(),
								now_state,
								test_pak_type::s_cal_type());

			owner_info_container_type owners;
			owners.insert(owner);
			YGGR_PP_TASK_CENTER_BACK_TASK(handler, prunner, error_maker_type::make_error(0),
											yggr::task_center::runtime_task_type::E_CAL_RESULT,
											owners, msg);
			return;

		}

		++i;
		std::cout << "-------------calculate " << i << "-----------------------"<< std::endl;
		std::cout << "id = " << cdt.id() << std::endl;

		owner_info_container_type owners;
		owners.insert(owner);
		YGGR_PP_TASK_CENTER_BACK_TASK(handler, prunner, error_maker_type::make_error(0),
										yggr::task_center::runtime_task_type::E_CAL_RESULT,
										owners, cdt);

		return;
	}

	template<typename Owner, typename Runner, typename Handler>
	void cal_register_back_msg_type(const Owner& owner,
										const proxy_register_back_msg_type& cdt,
										Runner* prunner,
										const Handler& handler)
	{
		typedef proxy_register_back_msg_type::reg_back_map_type reg_back_map_type;
		typedef reg_back_map_type::const_iterator citer_type;

		const reg_back_map_type& ref_back = cdt.reg_back_map();

		//Debug Code S
		bool bend = false;
		//Debug Code E

		for(citer_type i = ref_back.begin(), isize = ref_back.end(); i != isize; ++i)
		{
			std::cout << "register result id = " << i->first
						<< ", now_mode = " << i->second.first
						<< ", state = " << i->second.second << std::endl;

			//Debug Code S
			if(i->second.first == chg_state || i->second.second == 4)
			{
				bend = true;
			}
			//Debug Code E
		}

		//Debug code S
		if(now_state == chg_state || bend)
		{
			return;
		}

		proxy_mode_change_msg_type chg_msg;
		chg_msg.add_chg_data(test_pak_type::s_data_info(), chg_state);

		owner_info_container_type owners;
		owners.insert(owner);
		YGGR_PP_TASK_CENTER_BACK_TASK(handler, prunner, error_maker_type::make_error(0),
										yggr::task_center::runtime_task_type::E_CAL_RESULT,
										owners, chg_msg);
		//Debug code E
	}

	template<typename Owner, typename Runner, typename Handler>
	void cal_mode_change_back_msg_type(const Owner& owner,
										const proxy_mode_change_back_msg_type& cdt,
										Runner* prunner,
										const Handler& handler)
	{
		std::cout << "proxy server mode changed data_info = "
					<< cdt.now_data_info()
					<< " now mode is "
					<< cdt.now_mode() << std::endl;

		if(now_state == chg_state)
		{
			return;
		}

		proxy_unregister_msg_type unreg_msg;
		unreg_msg.add_unreg_data(test_pak_type::s_data_info());

		owner_info_container_type owners;
		owners.insert(owner);
		YGGR_PP_TASK_CENTER_BACK_TASK(handler, prunner, error_maker_type::make_error(0),
										yggr::task_center::runtime_task_type::E_CAL_RESULT,
										owners, unreg_msg);
	}

	template<typename Owner, typename Runner, typename Handler>
	void cal_proxy_unregister_back_msg_type(const Owner& owner,
										const proxy_unregister_back_msg_type& cdt,
										Runner* prunner,
										const Handler& handler)
	{
		typedef proxy_unregister_back_msg_type::unreg_back_map_type unreg_back_map_type;
		typedef unreg_back_map_type::const_iterator unreg_back_map_citer_type;
		const unreg_back_map_type& unreg_back_map = cdt.unreg_back_map();

		for(unreg_back_map_citer_type i = unreg_back_map.begin(), isize = unreg_back_map.end(); i != isize; ++i)
		{
			std::cout << "unreg data = "
						<< i->first
						<< " is right "
						<< (i->second? "true" : "false") << std::endl;
		}

	}
};

void out_input_menu(void)
{
	/*
	E_proxy_mode_monopolize,
		E_proxy_mode_source_hash,
		E_proxy_mode_blanace,
		E_proxy_mode_all,
	*/
	std::cout << "proxy_state:\n"
				<< "1: mode_monopolize\n"
				<< "2: mode_source_hash\n"
				<< "3: mode_blance\n"
				<< "4: mode_all" << std::endl;
}

int main(int argc, char* argv[])
{

	typedef yggr::ptr_single<task_center_type> task_center_single;
	typedef yggr::ptr_single<ctrl_center_type> ctrl_center_single;
	typedef yggr::ptr_single<yggr::log::yggr_exception_log_accesser_type> log_accesser_single;

	//std::cout << "please input num" << std::endl;
	//std::cin >> send_count;

	out_input_menu();
	std::cout << "please input now_state and chg_state" << std::endl;
	std::cin >> now_state >> chg_state;

	yggr::log::yggr_exception_log_accesser_type::init_type log_init; //memd to BOOST_PP_LOCAL_MACRO
	log_init.push_back("clt_nt_log_eins");
	log_init.push_back("clt_nt_log_zwei");

	//log_accesser_single::init_ins(log_init);

	yggr::log::yggr_exception_log_accesser_type::data_creator_type dc1("clt_nt_log_eins");
	yggr::log::yggr_exception_log_accesser_type::data_creator_type dc2("clt_nt_log_zwei");

	//log_accesser_single::get_ins()->register_msg(10061, dc1);
	//log_accesser_single::get_ins()->register_msg(10057, dc2);

	ctrl_center_single::init_ins();
	yggr::ptr_single<yggr::exception::exception>::init_ins();

	runner_shared_info_ptr_type shared_info_ptr(work_runner_type::create_work_runner_shared_info());

	if(!shared_info_ptr)
	{
		return -1;
	}

	boost::shared_ptr<Calculator> my_cal(new Calculator());
	shared_info_ptr->register_calculator(*my_cal);

	task_center_single::init_ins(shared_info_ptr, task_center_type::rst_saver_init_type());

	{
	    runner_shared_info_ptr_type tmp;
		shared_info_ptr.swap(tmp);
	}

	clt_ptr_single_type::init_ins(1,
									tcp_clt_handler_type::init_type(*(task_center_single::get_ins()),
																		tcp_clt_handler_type::link_handler_init_type()));

	clt_ptr_type pclt = clt_ptr_single_type::get_ins();

	ctrl_center_single::obj_ptr_type pctrl = ctrl_center_single::get_ins();

	// register network protocol
	pclt->register_network_protocol<test_pak_type>();
	pclt->register_network_protocol<proxy_register_msg_type>(yggr::network::network_direct_def_type::E_send_enable);
	pclt->register_network_protocol<proxy_register_back_msg_type>(yggr::network::network_direct_def_type::E_recv_enable);
	pclt->register_network_protocol<proxy_mode_change_msg_type>(yggr::network::network_direct_def_type::E_send_enable);
	pclt->register_network_protocol<proxy_mode_change_back_msg_type>(yggr::network::network_direct_def_type::E_recv_enable);
	pclt->register_network_protocol<proxy_unregister_msg_type>(yggr::network::network_direct_def_type::E_send_enable);
	pclt->register_network_protocol<proxy_unregister_back_msg_type>(yggr::network::network_direct_def_type::E_recv_enable);

	if(pctrl)
	{
		pctrl->register_controller(*pclt);
		//pctrl->register_dispatchers(*my_cal);
	}

	std::string ip("127.0.0.1");
	std::string port("9000");
	try
	{
		pclt->run();
		pclt->op_handler().connect(ip, port);
		std::cout << "started" << std::endl;

#	ifdef _DEBUG
		char cc = 0;
		std::cin >> cc;
#	else
		pclt->join();
		yggr::ptr_single<yggr::exception::exception>::get_ins()->join();
#	endif //_DEBUG

	}
	catch(const compatibility::stl_exception& e)
	{
        std::cerr << e.what() << std::endl;
	}
	catch(const boost::system::error_code& e)
	{
		std::cerr << e.message() << std::endl;
	}

	pclt->stop();
	//yggr::ptr_single<yggr::exception::exception>::get_ins()->stop();
	{
	    clt_ptr_type tmp;
		pclt.swap(tmp);
	}
	ctrl_center_single::uninstall();
	clt_ptr_single_type::uninstall();
	task_center_single::uninstall();

	std::cout << "stop end" << std::endl;
	yggr::ptr_single<yggr::exception::exception>::uninstall();

	return 0;
}

