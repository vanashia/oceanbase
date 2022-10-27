/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 *
 * config module
 */

#ifndef OCEANBASE_LIBOBCDC_CONFIG_H__
#define OCEANBASE_LIBOBCDC_CONFIG_H__

#include <map>
#include "share/ob_define.h"
#include "share/parameter/ob_parameter_macro.h"
#include "share/config/ob_common_config.h"    // ObInitConfigContainer

#include "ob_log_common.h"
#include "ob_log_fake_common_config.h"        // ObLogFakeCommonConfig

////////////// Define member variables of type INT, no limit on maximum value //////////////
// DEF: default value
// MIN: minimum value
//
// Note: DEF, MIN must be literal values, not variable names
#define T_DEF_INT_INFT(name, SCOPE, DEF, MIN, NOTE) \
    public: \
      static const int64_t default_##name = (DEF); \
      DEF_INT(name, SCOPE, #DEF, "[" #MIN ",]", NOTE);

////////////// Define INT type member variable //////////////
// DEF: default value
// MIN: minimum value
// MAX: maximum value
//
// Note: DEF, MIN, MAX must be literal values, not variable names
#define T_DEF_INT(name, SCOPE, DEF, MIN, MAX, NOTE) \
    public: \
      static const int64_t default_##name = (DEF); \
      static const int64_t max_##name = (MAX); \
      DEF_INT(name, SCOPE, #DEF, "[" #MIN "," #MAX "]", NOTE);

////////////// Define INT type member variable //////////////
// DEF: default value, 0 or 1
//
// Note: DEF must be a literal value, not a variable name
#define T_DEF_BOOL(name, SCOPE, DEF, NOTE) \
    public: \
      static const int64_t default_##name = DEF; \
      DEF_INT(name, SCOPE, #DEF, "[0,1]", NOTE);

namespace oceanbase
{
namespace libobcdc
{
class ObLogConfig : public common::ObInitConfigContainer
{
  typedef std::map<std::string, std::string> ConfigMap;

public:
  ObLogConfig() : inited_(false), common_config_(), config_file_buf1_(NULL), config_file_buf2_(NULL)
  {
  }

  virtual ~ObLogConfig() { destroy(); }

  int init();
  void destroy();
  static ObLogConfig &get_instance();

public:
  int check_all();
  void print() const;
  int load_from_buffer(const char *config_str,
      const int64_t config_str_len,
      const int64_t version = 0,
      const bool check_name = false);
  int load_from_file(const char *config_file,
      const int64_t version = 0,
      const bool check_name = false);
  int load_from_map(const ConfigMap& configs,
      const int64_t version = 0,
      const bool check_name = false);
  int dump2file(const char *file) const;

  common::ObCommonConfig &get_common_config() { return common_config_; }

  // remove quotes of cluster_url
  int format_cluster_url();

public:

#ifdef OB_CLUSTER_PARAMETER
#undef OB_CLUSTER_PARAMETER
#endif
#define OB_CLUSTER_PARAMETER(args...) args
  // Liboblog config.
  // max memory occupied by libobcdc: 20G
  DEF_CAP(memory_limit, OB_CLUSTER_PARAMETER, "20G", "[2G,]", "memory limit");
  // Preserve the lower bound of system memory in %, in the range of 10% ~ 80%
  // i.e.: ensure that the system memory remaining cannot be lower than this percentage based on the memory occupied by libobcdc
  DEF_INT(system_memory_avail_percentage_lower_bound, OB_CLUSTER_PARAMETER, "10", "[10, 80]", "system memory avail upper bound");
  DEF_CAP(tenant_manager_memory_upper_limit, OB_CLUSTER_PARAMETER, "5G", "[1G,]", "tenant manager memory upper limit");
  DEF_INT(dml_parser_thread_num, OB_CLUSTER_PARAMETER, "5", "[1,]", "DML parser thread number");
  DEF_INT(ddl_parser_thread_num, OB_CLUSTER_PARAMETER, "1", "[1,]", "DDL parser thread number");
  DEF_INT(sequencer_thread_num, OB_CLUSTER_PARAMETER, "5", "[1,]", "sequencer thread number");
  DEF_INT(sequencer_queue_length, OB_CLUSTER_PARAMETER, "102400", "[1,]", "sequencer queue length");
  DEF_INT(formatter_thread_num, OB_CLUSTER_PARAMETER, "10", "[1,]", "formatter thread number");
  DEF_INT(lob_data_merger_thread_num, OB_CLUSTER_PARAMETER, "2", "[1,]", "lob data merger thread number");
  DEF_CAP(batch_buf_size, OB_CLUSTER_PARAMETER, "20MB", "[2MB,]", "batch buf size");
  DEF_INT(batch_buf_count, OB_CLUSTER_PARAMETER, "10", "[5,]", "batch buf count");
  DEF_INT(storager_thread_num, OB_CLUSTER_PARAMETER, "10", "[1,]", "storager thread number");
  DEF_INT(storager_queue_length, OB_CLUSTER_PARAMETER, "102400", "[1,]", "storager queue length");
  DEF_INT(reader_thread_num, OB_CLUSTER_PARAMETER, "10", "[1,]", "reader thread number");
  DEF_INT(reader_queue_length, OB_CLUSTER_PARAMETER, "102400", "[1,]", "reader queue length");
  DEF_INT(cached_schema_version_count, OB_CLUSTER_PARAMETER, "32", "[1,]", "cached schema version count");
  DEF_INT(history_schema_version_count, OB_CLUSTER_PARAMETER, "16", "[1,]", "history schema version count");
  DEF_INT(resource_collector_thread_num, OB_CLUSTER_PARAMETER, "10", "[1,]", "resource collector thread number");
  DEF_INT(resource_collector_thread_num_for_br, OB_CLUSTER_PARAMETER, "7", "[1,]", "binlog record resource collector thread number");
  DEF_INT(instance_num, OB_CLUSTER_PARAMETER, "1", "[1,]", "store instance number");
  DEF_INT(instance_index, OB_CLUSTER_PARAMETER, "0", "[0,]", "store instance index, start from 0");
  DEF_INT(part_trans_task_prealloc_count, OB_CLUSTER_PARAMETER, "300000", "[1,]",
      "part trans task pre-alloc count");
  DEF_INT(part_trans_task_active_count_upper_bound, OB_CLUSTER_PARAMETER, "200000", "[1,]",
      "active part trans task count upper bound");
  DEF_INT(storager_task_count_upper_bound, OB_CLUSTER_PARAMETER, "1000", "[1,]",
      "storager task count upper bound");
  DEF_INT(storager_mem_percentage, OB_CLUSTER_PARAMETER, "2", "[1,]",
      "storager memory percentage");
  T_DEF_BOOL(skip_recycle_data, OB_CLUSTER_PARAMETER, 0, "0:not_skip, 1:skip")
  DEF_INT(part_trans_task_reusable_count_upper_bound, OB_CLUSTER_PARAMETER, "10240", "[1,]",
      "reusable parti trans task count upper bound");
  DEF_INT(ready_to_seq_task_upper_bound, OB_CLUSTER_PARAMETER, "20000", "[1,]",
      "ready to sequencer task count upper bound");
  DEF_INT(part_trans_task_dynamic_alloc, OB_CLUSTER_PARAMETER, "1", "[0,1]", "part trans task dynamic alloc");
  DEF_CAP(part_trans_task_page_size, OB_CLUSTER_PARAMETER, "8KB", "[1B,]", "part trans task page size");
  DEF_INT(part_trans_task_prealloc_page_count, OB_CLUSTER_PARAMETER, "20000", "[1,]",
      "part trans task prealloc page count");
  // Log_level=INFO in the startup scenario, and then optimize the schema to WARN afterwards
  DEF_STR(init_log_level, OB_CLUSTER_PARAMETER, "ALL.*:INFO;SHARE.SCHEMA:INFO", "log level: DEBUG, TRACE, INFO, WARN, USER_ERR, ERROR");
  DEF_STR(log_level, OB_CLUSTER_PARAMETER, "ALL.*:INFO;PALF.*:WARN;SHARE.SCHEMA:WARN", "log level: DEBUG, TRACE, INFO, WARN, USER_ERR, ERROR");
  // root server info for oblog, seperated by `;` between multi rootserver, a root server info format as `ip:rpc_port:sql_port`
  DEF_STR(rootserver_list, OB_CLUSTER_PARAMETER, "|", "OB RootServer list");
  DEF_STR(cluster_url, OB_CLUSTER_PARAMETER, "|", "OB configure url");
  DEF_STR(cluster_user, OB_CLUSTER_PARAMETER, "", "OB login user");
  DEF_STR(cluster_password, OB_CLUSTER_PARAMETER, "", "OB login password");
  DEF_STR(cluster_db_name, OB_CLUSTER_PARAMETER, "oceanbase", "OB login database name");
  DEF_STR(config_fpath, OB_CLUSTER_PARAMETER, DEFAULT_CONFIG_FPATN, "configuration file path");
  DEF_STR(timezone, OB_CLUSTER_PARAMETER, DEFAULT_TIMEZONE_INFO, "timezone info");
  // tenant_name.db_name.table_name
  DEF_STR(tb_white_list, OB_CLUSTER_PARAMETER, "*.*.*", "tb-select white list");
  DEF_STR(tb_black_list, OB_CLUSTER_PARAMETER, "|", "tb-select black list");
  // tenant_name.tablegroup_name
  DEF_STR(tablegroup_white_list, OB_CLUSTER_PARAMETER, "*.*", "tablegroup-select white list");
  DEF_STR(tablegroup_black_list, OB_CLUSTER_PARAMETER, "|", "tablegroup-select black list");

  DEF_STR(data_start_schema_version, OB_CLUSTER_PARAMETER, "|", "tenant:schema_version");
  // cluster id black list, using vertical line separation, for example cluster_id_black_list=100|200|300
  // Default value: 2^31 - 10000, this is a special cluster ID agreed in OCP for deleting historical data scenarios
  // libobcdc filters REDO data from deleted historical data scenarios by default
  DEF_STR(cluster_id_black_list, OB_CLUSTER_PARAMETER, "|", "cluster id black list");

  // minimum value of default cluster id blacklist value
  // The minimum value is: 2^31 - 10000 = 2147473648
  // This definition can only be a literal value
  T_DEF_INT_INFT(cluster_id_black_value_min, OB_CLUSTER_PARAMETER, 2147473648, 1, "min cluster id black value");

  // The maximum value of the default cluster id blacklist value
  // Maximum value: 2^31 - 1 = 2147483647
  // This definition can only be a literal value
  T_DEF_INT_INFT(cluster_id_black_value_max, OB_CLUSTER_PARAMETER, 2147483647, 1, "max cluster id black value");

  DEF_INT(log_entry_task_prealloc_count, OB_CLUSTER_PARAMETER, "100000", "[1,]", "log entry task pre-alloc count");

  DEF_INT(binlog_record_prealloc_count, OB_CLUSTER_PARAMETER, "100000", "[1,]", "binlog record pre-alloc count");

  DEF_STR(store_service_path, OB_CLUSTER_PARAMETER, "./storage", "store sevice path");

  // Whether to do ob version compatibility check
  // default value '0:not_skip'
  T_DEF_BOOL(skip_ob_version_compat_check, OB_CLUSTER_PARAMETER, 0, "0:not_skip, 1:skip")

  // default DFT_BR(LogRecordImpl), add DFT_BR_PB
  // passed in via IObLog::init interface
  // string LogMsgFactory::DFT_ColMeta = "ColMetaImpl";
  // string LogMsgFactory::DFT_TableMeta = "TableMetaImpl";
  // string LogMsgFactory::DFT_DBMeta = "DBMetaImpl";
  // string LogMsgFactory::DFT_METAS = "MetaDataCollectionsImpl";
  // string LogMsgFactory::DFT_LR = "LogRecordImpl";
  DEF_STR(drc_message_factory_binlog_record_type, OB_CLUSTER_PARAMETER, "LogRecordImpl", "LogMsgFactory::DFT_BR");

  // whether to check ObTraceId
  T_DEF_BOOL(need_verify_ob_trace_id, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  // ObTraceId, Configurable, default is default
  DEF_STR(ob_trace_id, OB_CLUSTER_PARAMETER, "default", "ob trace id");
  // Whether to turn on the verification mode
  // 1. verify dml unique id
  // 2. Verify ddl binlog record: schema version
  T_DEF_BOOL(enable_verify_mode, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  DEF_INT(enable_dump_pending_trans_info, OB_CLUSTER_PARAMETER, "0", "[0,1]",
      "enable dump pending transaction information");

  DEF_INT(log_clean_cycle_time_in_hours, OB_CLUSTER_PARAMETER, "24", "[0,]",
      "clean log cycle time in hours, 0 means not to clean log");
  DEF_INT(max_log_file_count, OB_CLUSTER_PARAMETER, "40", "[0,]", "max log file count, 0 means no limit");

  T_DEF_BOOL(skip_dirty_data, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  T_DEF_BOOL(skip_reversed_schema_verison, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  T_DEF_BOOL(skip_rename_tenant_ddl, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether to sort the list of participants within a distributed transaction
  // Scenario: online business does not need to enable this configuration item; this configuration item is only for obtest test scenario.
  // After each restart of obtest, the list of participants in the observer is random. In order to ensure consistent case results,
  // the list of participants needs to be sorted to ensure consistent results each time
  T_DEF_BOOL(sort_trans_participants, OB_CLUSTER_PARAMETER, 1, "0:disabled, 1:enabled");

  // Whether to allow globally unique indexes to be located in multiple instances
  // For example, in a count bin scenario, there is no strong reliance on global unique indexes to resolve dependencies
  T_DEF_BOOL(enable_global_unique_index_belong_to_multi_instance, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  ////////////////////////////// Fetcher config //////////////////////////////
  //
  // ------------------------------------------------------------------------
  //          Configurations that do not support dynamic changes
  // ------------------------------------------------------------------------
  // libobcdc support multiple working mode, default is storage
  // 1. storage: transaction data is stored, can support large transactions
  // 2. memory: transaction data is not stored, it means better performance, but may can not support large transactions
  DEF_STR(working_mode, OB_CLUSTER_PARAMETER, "storage", "libocdc working mode");
  T_DEF_INT_INFT(rocksdb_write_buffer_size, OB_CLUSTER_PARAMETER, 64, 16, "write buffer size[M]");

  T_DEF_INT_INFT(io_thread_num, OB_CLUSTER_PARAMETER, 4, 1, "io thread number");
  T_DEF_INT(idle_pool_thread_num, OB_CLUSTER_PARAMETER, 4, 1, 32, "idle pool thread num");
  T_DEF_INT(dead_pool_thread_num, OB_CLUSTER_PARAMETER, 1, 1, 32, "dead pool thread num");
  T_DEF_INT(stream_worker_thread_num, OB_CLUSTER_PARAMETER, 8, 1, 64, "stream worker thread num");
  T_DEF_INT(start_lsn_locator_thread_num, OB_CLUSTER_PARAMETER, 4, 1, 32, "start lsn locator thread num");
  T_DEF_INT_INFT(start_lsn_locator_locate_count, OB_CLUSTER_PARAMETER, 3, 1, "start lsn locator locate count");
  // Whether to skip the starting lsn positioning result consistency check, i.e. whether there is a positioning log bias scenario
  T_DEF_BOOL(skip_start_lsn_locator_result_consistent_check, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  T_DEF_INT_INFT(svr_stream_cached_count, OB_CLUSTER_PARAMETER, 16, 1, "cached svr stream object count");
  T_DEF_INT_INFT(fetch_stream_cached_count, OB_CLUSTER_PARAMETER, 16, 1, "cached fetch stream object count");

  // region
  DEF_STR(region, OB_CLUSTER_PARAMETER, "default_region", "OB region");

  // Number of globally cached RPC results
  T_DEF_INT_INFT(rpc_result_cached_count, OB_CLUSTER_PARAMETER, 16, 1, "cached rpc result object count");

  // Number of active ls count in memory
  // This value can be used as a reference for the number of data structure objects cached at the ls level
  T_DEF_INT_INFT(active_ls_count, OB_CLUSTER_PARAMETER, 10000, 1, "active ls count in memory");

  // Maximum number of ls currently supported
  T_DEF_INT_INFT(ls_count_upper_limit, OB_CLUSTER_PARAMETER, 2000000, 1, "max ls count supported");

  // Maximum number of threads using systable helper
  T_DEF_INT(access_systable_helper_thread_num, OB_CLUSTER_PARAMETER, 64, 48, 1024, "access systable helper thread num");

  // Global starting schema version, all tenants set to one version, only valid for schema non-split mode
  T_DEF_INT_INFT(global_data_start_schema_version, OB_CLUSTER_PARAMETER, 0, 0,
      "data start schema version for all tenant");
  // ------------------------------------------------------------------------


  // ------------------------------------------------------------------------
  //              configurations which supports dynamically modify
  // ------------------------------------------------------------------------
  T_DEF_INT_INFT(rs_sql_connect_timeout_sec, OB_CLUSTER_PARAMETER, 40, 1, "rootservice mysql connection timeout in seconds");
  T_DEF_INT_INFT(rs_sql_query_timeout_sec, OB_CLUSTER_PARAMETER, 30, 1, "rootservice mysql query timeout in seconds");
  T_DEF_INT_INFT(tenant_sql_connect_timeout_sec, OB_CLUSTER_PARAMETER, 40, 1, "tenant mysql connection timeout in seconds");
  T_DEF_INT_INFT(tenant_sql_query_timeout_sec, OB_CLUSTER_PARAMETER, 30, 1, "tenant mysql query timeout in seconds");
  T_DEF_INT_INFT(start_lsn_locator_rpc_timeout_sec, OB_CLUSTER_PARAMETER, 60, 1,
      "start lsn locator rpc timeout in seconds");
  T_DEF_INT_INFT(start_lsn_locator_batch_count, OB_CLUSTER_PARAMETER, 5, 1, "start lsn locator batch count");

  // server blacklist, default is|,means no configuration, support configuration single/multiple servers
  // Single: SEVER_IP1:PORT1
  // Multiple: SEVER_IP1:PORT1|SEVER_IP2:PORT2|SEVER_IP3:PORT3
  DEF_STR(server_blacklist, OB_CLUSTER_PARAMETER, "|", "server black list");
  DEF_STR(sql_server_blacklist, OB_CLUSTER_PARAMETER, "|", "sql server black list");

  T_DEF_INT_INFT(fetch_log_rpc_timeout_sec, OB_CLUSTER_PARAMETER, 15, 1, "fetch log rpc timeout in seconds");

  // Upper limit of progress difference between partitions, in seconds
  T_DEF_INT_INFT(progress_limit_sec_for_dml, OB_CLUSTER_PARAMETER, 300, 1, "dml progress limit in seconds");

  // The Sys Tenant is not filtered by default
  T_DEF_BOOL(enable_filter_sys_tenant, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // When all servers are added to the blacklist because of exceptions, the LS FetchCtx is dispatched into IDEL Pool mode.
  // If the RS servers continues to be disconnected, we cannot refresh new server list for FetchCtx by SQL. So The LS FetchCtx cannot fetch log.
  // If set enable_continue_use_cache_server_list is true, we can continue use cache server to fetch log.
  // A means of fault tolerance for LDG
  T_DEF_BOOL(enable_continue_use_cache_server_list, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  T_DEF_INT_INFT(progress_limit_sec_for_ddl, OB_CLUSTER_PARAMETER, 3600, 1, "ddl progress limit in seconds");

  // LS fetch progress update timeout in seconds
  // If the logs are not fetched after a certain period of time, the stream will be cut
  T_DEF_INT_INFT(ls_fetch_progress_update_timeout_sec, OB_CLUSTER_PARAMETER, 15, 1, "logstream fetch progress update timeout in seconds");
  // Timeout time for lagging replica logstreams
  //
  // If logs are not fetched for more than a certain period of time on a lagging copy, cut the stream
  T_DEF_INT_INFT(ls_fetch_progress_update_timeout_sec_for_lagged_replica, OB_CLUSTER_PARAMETER, 3, 1,
      "fetch progress update timeout for lagged replica in seconds");

  T_DEF_INT_INFT(log_router_background_refresh_interval_sec, OB_CLUSTER_PARAMETER, 10, 1,
                 "log_route_service background_refresh_time in seconds");
	// cache update interval of sys table __all_server
  T_DEF_INT_INFT(all_server_cache_update_interval_sec, OB_CLUSTER_PARAMETER, 5, 1,
			           "__all_server table cache update internal in seconds");

	// cache update interval of sys table __all_zone
  T_DEF_INT_INFT(all_zone_cache_update_interval_sec, OB_CLUSTER_PARAMETER, 5, 1,
			           "__all_zone table cache update internal in seconds");

  // pause fetcher
  T_DEF_BOOL(pause_fetcher, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Maximum number of tasks supported by the timer
  T_DEF_INT_INFT(timer_task_count_upper_limit, OB_CLUSTER_PARAMETER, 1024, 1, "max timer task count");
  // Timer task timing time
  T_DEF_INT_INFT(timer_task_wait_time_msec, OB_CLUSTER_PARAMETER, 100, 1, "timer task wait time in milliseconds");
  // SYS LS TASK OP TIMEOUT msec
  T_DEF_INT_INFT(sys_ls_task_op_timeout_msec, OB_CLUSTER_PARAMETER, 100, 1, "ddl data op timeout in milliseconds");

  // the upper limit observer takes  for the log rpc processing time
  // Print RPC chain statistics logs if this limit is exceeded
  T_DEF_INT_INFT(rpc_process_handler_time_upper_limit_msec, OB_CLUSTER_PARAMETER, 200, 1,
      "observer fetch log rpc process handler timer upper limit");

  // Survival time of server to blacklist, in seconds
  T_DEF_INT_INFT(blacklist_survival_time_sec, OB_CLUSTER_PARAMETER, 30, 1, "blacklist-server surival time in seconds");

  // The maximum time the server can be blacklisted, in minutes
  T_DEF_INT_INFT(blacklist_survival_time_upper_limit_min, OB_CLUSTER_PARAMETER, 4, 1, "blacklist-server survival time upper limit in minute");

  // The server is blacklisted in the logstream, based on the time of the current server service logstream - to decide whether to penalize the survival time
  // When the service time is less than a certain interval, a doubling-live-time policy is adopted
  // Unit: minutes
  T_DEF_INT_INFT(blacklist_survival_time_penalty_period_min, OB_CLUSTER_PARAMETER, 1, 1, "blacklist survival time punish interval in minute");

  // Blacklist history expiration time, used to delete history
  T_DEF_INT_INFT(blacklist_history_overdue_time_min, OB_CLUSTER_PARAMETER, 30, 10, "blacklist history overdue in minute");

  // Clear blacklist history period, unit: minutes
  T_DEF_INT_INFT(blacklist_history_clear_interval_min, OB_CLUSTER_PARAMETER, 20, 10, "blacklist history clear interval in minute");

  // Check the need for active cut-off cycles, in minutes
  T_DEF_INT_INFT(check_switch_server_interval_min, OB_CLUSTER_PARAMETER, 30, 1, "check switch server interval in minute");

  // Print the number of LSs with the slowest progress of the Fetcher module
  T_DEF_INT_INFT(print_fetcher_slowest_ls_num, OB_CLUSTER_PARAMETER, 10, 1, "print fetcher slowest ls num");

  // Maximum number of RPC results per RPC
  T_DEF_INT_INFT(rpc_result_count_per_rpc_upper_limit, OB_CLUSTER_PARAMETER, 16, 1,
      "max rpc result count per rpc");

  // Whether to print RPC processing information
  // Print every RPC processing
  // No printing by default
  T_DEF_BOOL(print_rpc_handle_info, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  T_DEF_BOOL(print_stream_dispatch_info, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // ------------------------------------------------------------------------
  // Print logstream heartbeat information
  T_DEF_BOOL(print_ls_heartbeat_info, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  // Print logstream service information
  T_DEF_BOOL(print_ls_serve_info, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  // Print logstream not in service information
  T_DEF_BOOL(print_participant_not_serve_info, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  // Print the svr list of each logstream update, off by default
  T_DEF_BOOL(print_ls_server_list_update_info, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  // Whether to sequentially output within a transaction
  // Not on by default (participatn-by-participant output)
  T_DEF_BOOL(enable_output_trans_order_by_sql_operation, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");
  // redo dispatcher memory limit
  DEF_CAP(redo_dispatcher_memory_limit, OB_CLUSTER_PARAMETER, "1G", "[128M,]", "redo dispatcher memory limit");
  // redo diepatcher memory limit ratio for output br by sql operation(compare with redo_dispatcher_memory_limit)
  T_DEF_INT_INFT(redo_dispatched_memory_limit_exceed_ratio, OB_CLUSTER_PARAMETER, 2, 1,
      "redo_dispatcher_memory_limit ratio for output by sql operation order");
  // sorter thread num
  T_DEF_INT(msg_sorter_thread_num, OB_CLUSTER_PARAMETER, 1, 1, 32, "trans msg sorter thread num");
  // sorter thread
  T_DEF_INT_INFT(msg_sorter_task_count_upper_limit, OB_CLUSTER_PARAMETER, 200000, 1, "trans msg sorter thread num");

  // ------------------------------------------------------------------------
  // Test mode, used only in obtest and other test tool scenarios
  T_DEF_BOOL(test_mode_on, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether check tenant status for each schema request with tenant_id under test mode, default disabled
  T_DEF_BOOL(test_mode_force_check_tenant_status, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether to allow the output of the current transaction's major_version in test mode, not allowed by default
  T_DEF_BOOL(test_output_major_version, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // The number of times sqlServer cannot get the rs list in test mode
  T_DEF_INT_INFT(test_mode_block_sqlserver_count, OB_CLUSTER_PARAMETER, 0, 0,
      "mock times of con't get rs list under test mode");

  // Number of REDO logs ignored in test mode
  T_DEF_INT_INFT(test_mode_ignore_redo_count, OB_CLUSTER_PARAMETER, 0, 0,
      "ignore redo log count under test mode");

  // Test checkpoint mode, used only in obtest and other test tool scenarios
  T_DEF_BOOL(test_checkpoint_mode_on, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // test mode, whether to block the participant list confirmation process, and if so, how long to block
  // Equal to 0, means no blocking
  // greater than 0 means blocking time in seconds
  //
  // The purpose is to delay the participant list confirmation process and wait for the participant information to be confirmed before operating
  T_DEF_INT_INFT(test_mode_block_verify_participants_time_sec, OB_CLUSTER_PARAMETER, 0, 0,
      "time in seconds to block to verify participants list");

  // test mode, whether blocking committer processing task, if blocking, how long to block
  // equal to 0, means no blocking
  // greater than 0, means blocking time in seconds
  //
  // test drop tenant, committer processing task delayed, wait long enough to ensure tenant structure can be deleted
  T_DEF_INT_INFT(test_mode_block_committer_handle_time_sec, OB_CLUSTER_PARAMETER, 0, 0,
      "time in seconds to block to verify tenant has been dropped");

  // In test mode, set the upper limit of the number of tasks consumed by the committer at one time
  T_DEF_INT_INFT(test_mode_committer_handle_trans_count_upper_limit, OB_CLUSTER_PARAMETER, 0, 0,
      "commiter handle trans count upper limit under test mode");

  // test mode, whether blocking create table DDL, if blocking, how long blocking
  // Equal to 0, means no blocking
  // greater than 0 means blocking time in seconds
  //
  // The purpose is to block the create table DDL, test PG filtering
  T_DEF_INT_INFT(test_mode_block_create_table_ddl_sec, OB_CLUSTER_PARAMETER, 0, 0,
      "time in seconds to block to create table");

  // test mode, whether blocking alter table DDL, if blocking, how long blocking
  // Equal to 0, means no blocking
  // greater than 0 means blocking time in seconds
  //
  // The purpose is to block alter table DDL, test PG filtering
  T_DEF_INT_INFT(test_mode_block_alter_table_ddl_sec, OB_CLUSTER_PARAMETER, 0, 0,
      "time in seconds to block to alter table");

  // test mode, whether blocking filter row process, if blocking, how long to block
  // Equal to 0, means no blocking
  // greater than 0, means blocking time in seconds
  //
  // The purpose is to block filter row, test PG filtering
  T_DEF_INT_INFT(test_mode_block_parser_filter_row_data_sec, OB_CLUSTER_PARAMETER, 0, 0,
      "time in seconds to block to filter row data");

  // INNER_HEARTBEAT_INTERVAL
  T_DEF_INT_INFT(output_inner_heartbeat_interval_msec, OB_CLUSTER_PARAMETER, 100, 1, "output heartbeat interval in micro seconds");

  // Output heartbeat interval to external, default 1s
  T_DEF_INT_INFT(output_heartbeat_interval_msec, OB_CLUSTER_PARAMETER, 1000, 1, "output heartbeat interval in seconds");

  // Whether to have incremental backup mode
  // Off by default; if it is, then incremental backup mode
  T_DEF_BOOL(enable_backup_mode, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether to expose no primary key table hidden primary key to the public
  // 1. DRC linking is off by default; if it is in effect, output the hidden primary key
  // 2. Backup is on by default
  T_DEF_BOOL(enable_output_hidden_primary_key, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Ignore inconsistencies in the number of HBase mode put columns or not
  // Do not skip by default
  T_DEF_BOOL(skip_hbase_mode_put_column_count_not_consistency, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether to ignore the transaction log for exceptions
  // Do not skip by default
  T_DEF_BOOL(skip_abnormal_trans_log, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether to allow hbase schema to take effect
  // off by default; if it is, then convert the hbase table T timestamp field to a positive number
  T_DEF_BOOL(enable_hbase_mode, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether to allow timestamp->utc integer time
  // 1. off by default, the timestamp field is converted to year-month-day format based on time zone information.
  // 2. When configured on, the timestamp field is synchronized to integer
  T_DEF_BOOL(enable_convert_timestamp_to_unix_timestamp, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Whether to output invisible columns externally
  // 1. DRC link is off by default; if valid, output hidden primary key
  // 2. Backup is on by default
  T_DEF_BOOL(enable_output_invisible_column, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // The point in time when the sql server used for querying in SYSTABLE HELPER changes, i.e., the periodic rotation of the sql server
  T_DEF_INT_INFT(sql_server_change_interval_sec, OB_CLUSTER_PARAMETER, 60, 1,
      "change interval of sql server in seconds");

  // Check if version matches, default 600s
  T_DEF_INT_INFT(cluster_version_refresh_interval_sec, OB_CLUSTER_PARAMETER, 600, 1, "cluster version refresh interval in seconds");

  // Oracle mode table/database may have case, and case sensitive
  // default enable_oracle_mode_match_case_sensitive=0 whitelist match is consistent with mysql behavior, match is not sensitive
  // enable_oracle_mode_match_case_sensitive=1 allow match sensitive
  T_DEF_BOOL(enable_oracle_mode_match_case_sensitive, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

   // Switch: Whether to format the module to print the relevant logs
  // No printing by default
  T_DEF_BOOL(enable_formatter_print_log, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // Switch: Whether to enable SSL authentication: including MySQL and RPC
  // Disabled by default
  T_DEF_BOOL(ssl_client_authentication, OB_CLUSTER_PARAMETER, 0, "0:disabled, 1:enabled");

  // SSL external kms info
  // 1. Local file mode: ssl_external_kms_info=file
  // 2. BKMI mode: ssl_external_kms_info=hex(...)
  DEF_STR(ssl_external_kms_info, OB_CLUSTER_PARAMETER, "|", "ssl external kms info");

#undef OB_CLUSTER_PARAMETER

private:
  static const int64_t OBLOG_MAX_CONFIG_LENGTH = 5 * 1024 * 1024;  // 5M

private:
  bool                  inited_;
  ObLogFakeCommonConfig common_config_;

  // for load_from_file
  char                  *config_file_buf1_;
  // for load_from_buffer
  char                  *config_file_buf2_;

private:
  DISALLOW_COPY_AND_ASSIGN(ObLogConfig);
};

#define TCONF (::oceanbase::libobcdc::ObLogConfig::get_instance())

} // namespace libobcdc
} // namespace oceanbase
#endif /* OCEANBASE_LIBOBCDC_CONFIG_H__ */
