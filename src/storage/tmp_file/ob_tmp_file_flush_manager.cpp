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
 */

#define USING_LOG_PREFIX STORAGE

#include "storage/tmp_file/ob_tmp_file_flush_manager.h"
#include "storage/tmp_file/ob_tmp_file_manager.h"
#include "storage/blocksstable/ob_block_manager.h"
#include "storage/tmp_file/ob_tmp_file_flush_list_iterator.h"

namespace oceanbase
{
namespace tmp_file
{

ObTmpFileFlushManager::ObTmpFileFlushManager(ObTmpFilePageCacheController &pc_ctrl)
  : is_inited_(false),
    flush_ctx_(),
    pc_ctrl_(pc_ctrl),
    tmp_file_block_mgr_(pc_ctrl.get_tmp_file_block_manager()),
    task_allocator_(pc_ctrl.get_task_allocator()),
    write_buffer_pool_(pc_ctrl.get_write_buffer_pool()),
    evict_mgr_(pc_ctrl.get_eviction_manager()),
    flush_priority_mgr_(pc_ctrl.get_flush_priority_mgr())
{
}

int ObTmpFileFlushManager::init()
{
  int ret = OB_SUCCESS;
  if (IS_INIT) {
    ret = OB_INIT_TWICE;
    STORAGE_LOG(WARN, "ObTmpFileFlushManager inited twice", KR(ret));
  } else if (OB_FAIL(flush_ctx_.init())) {
    STORAGE_LOG(WARN, "fail to init flush ctx", KR(ret));
  } else {
    is_inited_ = true;
  }
  return ret;
}

void ObTmpFileFlushManager::destroy()
{
  is_inited_ = false;
  flush_ctx_.destroy();
}

int ObTmpFileFlushManager::alloc_flush_task(ObTmpFileFlushTask *&flush_task)
{
  int ret = OB_SUCCESS;
  flush_task = nullptr;

  const int64_t BLOCK_SIZE = OB_SERVER_BLOCK_MGR.get_macro_block_size();
  void *task_buf = nullptr;
  if (OB_ISNULL(task_buf = task_allocator_.alloc(sizeof(ObTmpFileFlushTask)))) {
    ret = OB_ALLOCATE_MEMORY_FAILED;
    STORAGE_LOG(WARN, "fail to allocate memory for flush callback", KR(ret));
  } else {
    flush_task = new (task_buf) ObTmpFileFlushTask();
  }
  return ret;
}

int ObTmpFileFlushManager::free_flush_task(ObTmpFileFlushTask *flush_task)
{
  int ret = OB_SUCCESS;
  if (OB_ISNULL(flush_task)) {
    ret = OB_ERR_UNEXPECTED;
    STORAGE_LOG(WARN, "flush task ptr is null", KR(ret));
  } else {
    LOG_DEBUG("free flush task", KPC(flush_task));
    flush_task->~ObTmpFileFlushTask();
    task_allocator_.free(flush_task);
  }
  return ret;
}

int ObTmpFileFlushManager::notify_write_back_failed(ObTmpFileFlushTask *flush_task)
{
  int ret = OB_SUCCESS;
  if (OB_ISNULL(flush_task)) {
    ret = OB_ERR_UNEXPECTED;
    STORAGE_LOG(WARN, "flush task ptr is null", KR(ret));
  } else if (OB_FAIL(tmp_file_block_mgr_.write_back_failed(flush_task->get_block_index()))) {
    STORAGE_LOG(ERROR, "fail to notify tmp file block write back failed", KR(ret), KPC(flush_task));
  }
  return ret;
}

// release whole block
int ObTmpFileFlushManager::free_tmp_file_block(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;
  int64_t block_index = flush_task.get_block_index();
  if (ObTmpFileGlobal::INVALID_TMP_FILE_BLOCK_INDEX == block_index) {
    // do nothing
  } else if (OB_FAIL(tmp_file_block_mgr_.release_tmp_file_page(block_index,
                                                               0/*begin_page_id*/,
                                                               ObTmpFileGlobal::BLOCK_PAGE_NUMS))) {
    STORAGE_LOG(WARN, "fail to remove tmp file block", KR(ret), K(block_index), K(flush_task));
  }
  return ret;
}

void ObTmpFileFlushManager::init_flush_level_()
{
  int64_t dirty_page_percentage = write_buffer_pool_.get_dirty_page_percentage();
  if (pc_ctrl_.is_flush_all_data()) { // only for unittest
    flush_ctx_.set_state(FlushCtxState::FSM_F1);
  } else {
    if (FLUSH_WATERMARK_F1 <= dirty_page_percentage) {
      flush_ctx_.set_state(FlushCtxState::FSM_F1);
    } else if (FLUSH_WATERMARK_F2 <= dirty_page_percentage) {
      flush_ctx_.set_state(FlushCtxState::FSM_F2);
    } else if (FLUSH_WATERMARK_F3 <= dirty_page_percentage) {
      flush_ctx_.set_state(FlushCtxState::FSM_F3);
    } else if (FLUSH_WATERMARK_F4 <= dirty_page_percentage) {
      flush_ctx_.set_state(FlushCtxState::FSM_F4);
    } else if (FLUSH_WATERMARK_F5 <= dirty_page_percentage) {
      flush_ctx_.set_state(FlushCtxState::FSM_F5);
    } else {
      flush_ctx_.set_state(FlushCtxState::FSM_FINISHED);
    }
  }
}

void ObTmpFileFlushManager::advance_flush_level_(const int ret_code)
{
  int64_t dirty_page_percentage = write_buffer_pool_.get_dirty_page_percentage();

  if (pc_ctrl_.is_flush_all_data()) {  // only for unittest
    if (OB_SUCCESS == ret_code) {
      // continue to flush to OB_ITER_END, do nothing
    } else if (OB_ITER_END == ret_code) {
      inner_advance_flush_level_without_checking_watermark_();
    } else {
      flush_ctx_.set_state(FlushCtxState::FSM_FINISHED);
    }
  } else {
    if (OB_SUCCESS == ret_code) {
      // if lower than low_watermark or reach expect_flush_size, terminate flush process
      if (get_low_watermark_(flush_ctx_.get_state()) >= dirty_page_percentage
               || flush_ctx_.get_actual_flush_size() >= flush_ctx_.get_expect_flush_size()) {
        flush_ctx_.set_state(FlushCtxState::FSM_FINISHED);
      }
    } else if (OB_ITER_END == ret_code) {
      inner_advance_flush_level_();
    } else {
      flush_ctx_.set_state(FlushCtxState::FSM_FINISHED);
    }
  }

  LOG_DEBUG("advance_flush_level_", K(flush_ctx_));
}

int64_t ObTmpFileFlushManager::get_low_watermark_(FlushCtxState state)
{
  int ret = OB_SUCCESS;
  int64_t low_watermark = 100;
  switch(state) {
    case FlushCtxState::FSM_F1:
      low_watermark = FLUSH_LOW_WATERMARK_F1;
      break;
    case FlushCtxState::FSM_F2:
      low_watermark = FLUSH_LOW_WATERMARK_F2;
      break;
    case FlushCtxState::FSM_F3:
      low_watermark = FLUSH_LOW_WATERMARK_F3;
      break;
    case FlushCtxState::FSM_F4:
      low_watermark = FLUSH_LOW_WATERMARK_F4;
      break;
    case FlushCtxState::FSM_F5:
      low_watermark = FLUSH_LOW_WATERMARK_F5;
      break;
    case FlushCtxState::FSM_FINISHED:
      break;
    default:
      STORAGE_LOG(WARN, "unexpected flush state", K(state), K(flush_ctx_));
      break;
  }
  return low_watermark;
}

void ObTmpFileFlushManager::inner_advance_flush_level_()
{
  if (flush_ctx_.get_actual_flush_size() >= flush_ctx_.get_expect_flush_size()) {
    flush_ctx_.set_state(FlushCtxState::FSM_FINISHED);
  }

  int64_t dirty_page_percentage = write_buffer_pool_.get_dirty_page_percentage();
  if (FLUSH_WATERMARK_F2 <= dirty_page_percentage && flush_ctx_.get_state() < FlushCtxState::FSM_F2) {
    flush_ctx_.set_state(FlushCtxState::FSM_F2);
  } else if (FLUSH_WATERMARK_F3 <= dirty_page_percentage && flush_ctx_.get_state() < FlushCtxState::FSM_F3) {
    flush_ctx_.set_state(FlushCtxState::FSM_F3);
  } else if (FLUSH_WATERMARK_F4 <= dirty_page_percentage && flush_ctx_.get_state() < FlushCtxState::FSM_F4) {
    flush_ctx_.set_state(FlushCtxState::FSM_F4);
  } else if (FLUSH_WATERMARK_F5 <= dirty_page_percentage && flush_ctx_.get_state() < FlushCtxState::FSM_F5) {
    flush_ctx_.set_state(FlushCtxState::FSM_F5);
  } else {
    flush_ctx_.set_state(FlushCtxState::FSM_FINISHED);
  }
}

// only for unittest
void ObTmpFileFlushManager::inner_advance_flush_level_without_checking_watermark_()
{
  switch(flush_ctx_.get_state()) {
    case FlushCtxState::FSM_F1:
      flush_ctx_.set_state(FlushCtxState::FSM_F2);
      break;
    case FlushCtxState::FSM_F2:
      flush_ctx_.set_state(FlushCtxState::FSM_F3);
      break;
    case FlushCtxState::FSM_F3:
      flush_ctx_.set_state(FlushCtxState::FSM_F4);
      break;
    case FlushCtxState::FSM_F4:
      flush_ctx_.set_state(FlushCtxState::FSM_F5);
      break;
    case FlushCtxState::FSM_F5:
      flush_ctx_.set_state(FlushCtxState::FSM_FINISHED);
      break;
    default:
      break;
  }
}

// Generate a set of flush tasks based on the flushing level to flush dirty pages,
// and attempt to advance the task status to TFFT_WAIT (waiting for asynchronous IO).
// When OB_ALLOCATE_TMP_FILE_PAGE_FAILED error occurs during flushing,
// a special flush task is generated to flush meta pages and terminate the current round of flushing.
// Setting fast_flush_meta to true by the caller also triggers this process.
int ObTmpFileFlushManager::flush(ObSpLinkQueue &flushing_queue,
                                 ObTmpFileFlushMonitor &flush_monitor,
                                 const int64_t expect_flush_size,
                                 const bool is_flush_meta_tree)
{
  int ret = OB_SUCCESS;
  bool fast_flush_meta = is_flush_meta_tree;

  init_flush_level_();

  if (FlushCtxState::FSM_FINISHED == flush_ctx_.get_state() && !fast_flush_meta) {
    ret = OB_SUCCESS;
  } else if (OB_FAIL(flush_ctx_.prepare_flush_ctx(expect_flush_size, &flush_priority_mgr_, &flush_monitor))) {
    STORAGE_LOG(WARN, "fail to prepare flush iterator", KR(ret), K(flush_ctx_));
  } else {
    while (OB_SUCC(ret) && !flush_ctx_.is_fail_too_many()
                        && (FlushCtxState::FSM_FINISHED != flush_ctx_.get_state() || fast_flush_meta)) {
      ObTmpFileFlushTask *flush_task = nullptr;
      if (OB_FAIL(handle_alloc_flush_task_(fast_flush_meta, flush_task))) {
        STORAGE_LOG(WARN, "fail to alloc flush task", KR(ret), K(flush_ctx_));
      } else {
        flush_ctx_.inc_create_flush_task_cnt();
        flushing_queue.push(flush_task);
        STORAGE_LOG(DEBUG, "create new flush task", K(fast_flush_meta), KPC(flush_task), K(flush_ctx_));

        FlushState state = ObTmpFileFlushTask::TFFT_INITED;
        FlushState next_state = state;
        do {
          next_state = state = flush_task->get_state();
          if (OB_FAIL(drive_flush_task_prepare_(*flush_task, state, next_state))) {
            STORAGE_LOG(WARN, "fail to drive flush task prepare", KR(ret), K(flush_ctx_));
          } else if (flush_task->get_state() >= next_state) {
            ret = OB_ERR_UNEXPECTED;
            STORAGE_LOG(WARN, "unexpected flush state after drive task succ", KR(ret), K(flush_ctx_), K(state));
          } else if (OB_FAIL(advance_status_(*flush_task, next_state))) {
            STORAGE_LOG(WARN, "fail to advance status",
                        KR(ret), K(flush_ctx_), K(flush_task->get_state()), K(state), K(next_state));
          }
        } while (OB_SUCC(ret) && FlushState::TFFT_WAIT != next_state);

        STORAGE_LOG(DEBUG, "drive flush task finished", KR(ret), K(fast_flush_meta), KPC(flush_task), K(flush_ctx_));
        bool recorded_as_prepare_finished = false;
        flush_ctx_.update_actual_flush_size(*flush_task);
        flush_ctx_.try_update_prepare_finished_cnt(*flush_task, recorded_as_prepare_finished);
        if (recorded_as_prepare_finished) {
          flush_task->mark_recorded_as_prepare_finished();
        }
        if (ObTmpFileFlushTask::TFFT_FILL_BLOCK_BUF < flush_task->get_state()) {
          flush_ctx_.record_flush_task(flush_task->get_data_length()); // maintain statistics
        }
        if (flush_task->get_is_fast_flush_tree()) {
          if (OB_FAIL(ret)) {
            STORAGE_LOG(ERROR, "fail to execute fast_flush_tree_page flush task to TFFT_WAIT", KR(ret), KPC(flush_task));
          }
          break;  // generate only one fast_flush_tree_page_ task to avoid excessive flushing of the meta
        }
        if (OB_ALLOCATE_TMP_FILE_PAGE_FAILED == ret){
          if (flush_task->get_state() == FlushState::TFFT_INSERT_META_TREE) {
            STORAGE_LOG(WARN, "fail to insert meta tree, generating fast_flush_meta task", KR(ret));
            fast_flush_meta = true;   // set this flag generate fast_flush_tree_page_ task in the next loop
            ret = OB_SUCCESS;
          } else {
            STORAGE_LOG(ERROR, "flush task is not in TFFT_INSERT_META_TREE state", KPC(flush_task));
          }
        }
      }
    }

    if (!flushing_queue.is_empty()) {
      STORAGE_LOG(DEBUG, "ObTmpFileFlushManager flush finish", KR(ret), K(fast_flush_meta), K(flush_ctx_));
    }
    if (OB_FAIL(ret) && !flushing_queue.is_empty()) {
      ret = OB_SUCCESS; // ignore error if generate at least 1 task
    }
    flush_ctx_.clear_flush_ctx(flush_priority_mgr_);
  }
  return ret;
}

// skip flush level, copy meta tree pages directly
int ObTmpFileFlushManager::fast_fill_block_buf_with_meta_(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;

  if (OB_FAIL(inner_fill_block_buf_(flush_task, FlushCtxState::FSM_F4, true/*is_meta*/, false/*flush_tail*/))) {
    LOG_WARN("fail to fast generate flush meta task in flush policy f4", KR(ret), K(flush_task));
  }

  if (flush_task.get_data_length() < FAST_FLUSH_TREE_PAGE_NUM * ObTmpFileGlobal::PAGE_SIZE) { // ignore error code and try F5
    if (OB_FAIL(inner_fill_block_buf_(flush_task, FlushCtxState::FSM_F5, true/*is_meta*/, true/*flush_tail*/))) {
      LOG_WARN("fail to fast generate flush meta task in flush policy f5", KR(ret), K(flush_task));
    }
  }

  if (OB_FAIL(ret)) {
    if (OB_ITER_END == ret && flush_task.get_flush_infos().size() != 0) {
      // ignore OB_ITER_END error code to continue to next stage
      LOG_INFO("fast_fill_block_buf_with_meta_ iterator reach end, ignore error code to continue",
          KR(ret), K(flush_task), K(flush_ctx_));
      ret = OB_SUCCESS;
    } else {
      LOG_WARN("fail to fast fill block buf with meta", KR(ret), K(flush_task), K(flush_ctx_));
    }
  }
  return ret;
}

int ObTmpFileFlushManager::fill_block_buf_(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;
  switch(flush_ctx_.get_state()) {
    case FlushCtxState::FSM_F1:
      if (!flush_task.is_full() && FlushCtxState::FSM_FINISHED != flush_ctx_.get_state()
            && OB_FAIL(inner_fill_block_buf_(flush_task, flush_ctx_.get_state(),
                                             false/*is_meta*/, false/*flush_tail*/))) {
        if (OB_ITER_END != ret) {
          STORAGE_LOG(WARN, "fail to generate flush data task in flush policy f1", KR(ret), K(flush_task));
        }
      }
      advance_flush_level_(ret);
      // go through
    case FlushCtxState::FSM_F2:
      if (!flush_task.is_full() && FlushCtxState::FSM_FINISHED != flush_ctx_.get_state()
            && OB_FAIL(inner_fill_block_buf_(flush_task, flush_ctx_.get_state(),
                                             false/*is_meta*/, false/*flush_tail*/))) {
        if (OB_ITER_END != ret) {
          STORAGE_LOG(WARN, "fail to generate flush data task in flush policy f2", KR(ret), K(flush_task));
        }
      }
      advance_flush_level_(ret);
      // go through
    case FlushCtxState::FSM_F3:
      if (!flush_task.is_full() && FlushCtxState::FSM_FINISHED != flush_ctx_.get_state()
            && OB_FAIL(inner_fill_block_buf_(flush_task, flush_ctx_.get_state(),
                                             false/*is_meta*/, true/*flush_tail*/))) {
        if (OB_ITER_END != ret) {
          STORAGE_LOG(WARN, "fail to generate flush data task in flush policy f3", KR(ret), K(flush_task));
        }
      }
      advance_flush_level_(ret);
      // break here to forbid flush data pages and meta pages in the same flush task
      // to prevent 1 flush task contains all of meta page that can be flushed,
      // but is stuck in TFFT_INSERT_META_TREE state and new task has no meta pages to flush
      break;
    case FlushCtxState::FSM_F4:
      if (!flush_task.is_full() && FlushCtxState::FSM_FINISHED != flush_ctx_.get_state()
            && OB_FAIL(inner_fill_block_buf_(flush_task, flush_ctx_.get_state(),
                                             true/*is_meta*/, false/*flush_tail*/))) {
        if (OB_ITER_END != ret) {
          STORAGE_LOG(WARN, "fail to generate flush meta task in flush policy f4", KR(ret), K(flush_task));
        }
      }
      advance_flush_level_(ret);
      // go through
    case FlushCtxState::FSM_F5:
      if (!flush_task.is_full() && FlushCtxState::FSM_FINISHED != flush_ctx_.get_state()
            && OB_FAIL(inner_fill_block_buf_(flush_task, flush_ctx_.get_state(),
                                             true/*is_meta*/, true/*flush_tail*/))) {
        if (OB_ITER_END != ret) {
          STORAGE_LOG(WARN, "fail to generate flush meta task in flush policy f5", KR(ret), K(flush_task));
        }
      }
      advance_flush_level_(ret);
      // go through
    case FlushCtxState::FSM_FINISHED:
      // do nothing
      break;
    default:
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(ERROR, "unexpected flush state", KR(ret), K(flush_ctx_));
      break;
  }
  if (OB_ITER_END == ret && flush_task.get_flush_infos().size() != 0) {
    // ignore OB_ITER_END error code to continue TFFT_INSERT_META_TREE state
    STORAGE_LOG(DEBUG, "fill_block_buf_ iterator reach end, ignore error code to continue",
                KR(ret), K(flush_task), K(flush_ctx_));
    ret = OB_SUCCESS;
  }
  return ret;
}

int ObTmpFileFlushManager::inner_fill_block_buf_(
    ObTmpFileFlushTask &flush_task,
    const FlushCtxState flush_stage,
    const bool is_meta,
    const bool flush_tail)
{
  int ret = OB_SUCCESS;
  const int64_t BLOCK_SIZE = OB_SERVER_BLOCK_MGR.get_macro_block_size();
  bool fail_too_many = false;

  ObArray<ObTmpFileBatchFlushContext::ObTmpFileFlushFailRecord> &flush_failed_array = flush_ctx_.get_flush_failed_array();
  ObTmpFileFlushListIterator &iter = flush_ctx_.get_flush_list_iterator();
  bool tmp_is_meta = false;
  ObTmpFileHandle file_handle;
  while (OB_SUCC(ret) && !fail_too_many && !flush_task.is_full()) {
    if (OB_FAIL(iter.next(flush_stage, tmp_is_meta, file_handle))) {
      if (OB_ITER_END != ret) {
        STORAGE_LOG(WARN, "fail to get file from iterator",
            KR(ret), K(flush_stage), K(is_meta), K(flush_tail), K(flush_ctx_));
      }
    } else if (OB_ISNULL(file_handle.get())) {
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "file handle is nullptr", KR(ret));
    } else {
      ObSharedNothingTmpFile &file = *file_handle.get();
      if (file.is_deleting()) {
        STORAGE_LOG(INFO, "file is deleted while generating flush task", K(file));
      } else {
        STORAGE_LOG(DEBUG, "try to copy data from file", K(file.get_fd()), K(is_meta), K(flush_tail), K(file));
        int64_t last_idx = -1;
        bool copy_flush_info_fail = false;
        ObArray<ObTmpFileFlushInfo> &flush_infos = flush_task.get_flush_infos();
        const int64_t origin_info_cnt = flush_infos.count();
        ObTmpFileSingleFlushContext file_flush_ctx(file_handle);
        // push back first to prevent array resizing or
        // hash map allocate node failure AFTER copying the file data
        if (FAILEDx(get_or_create_file_in_ctx_(file.get_fd(), file_flush_ctx))) {
          STORAGE_LOG(WARN, "fail to get or create file in file flush ctx", KR(ret), K(file));
        } else if (OB_FAIL(flush_infos.push_back(ObTmpFileFlushInfo()))) {
          STORAGE_LOG(WARN, "fail to insert flush info", KR(ret), K(file), K(flush_task));
        } else if (FALSE_IT(last_idx = flush_infos.size() - 1)) {
        } else if (!is_meta && OB_FAIL(file.generate_data_flush_info(flush_task, flush_infos.at(last_idx),
                                          file_flush_ctx.data_ctx_, flush_ctx_.get_flush_sequence(), flush_tail))) {
          STORAGE_LOG(WARN, "fail to generate flush data info", KR(ret), K(flush_task),
                      K(flush_stage), K(is_meta), K(flush_tail), K(file));
          copy_flush_info_fail = true;
        } else if (is_meta && OB_FAIL(file.generate_meta_flush_info(flush_task, flush_infos.at(last_idx),
                                          file_flush_ctx.meta_ctx_, flush_ctx_.get_flush_sequence(), flush_tail))) {
          STORAGE_LOG(WARN, "fail to generate flush meta info", KR(ret), K(flush_task),
                      K(flush_stage), K(is_meta), K(flush_tail), K(file));
          copy_flush_info_fail = true;
        } else {
          if (!(flush_tail || (is_meta && file_flush_ctx.meta_ctx_.is_meta_reach_end_))) {
            file.reinsert_flush_node(is_meta);
          }
          UpdateFlushCtx update_op(file_flush_ctx);
          if (OB_FAIL(flush_ctx_.get_file_ctx_hash().set_or_update(file.get_fd(), file_flush_ctx, update_op))) {
            // if update fails, the copy offset will be incorrect when the file is flushed a second time in the same round
            STORAGE_LOG(ERROR, "fail to set flush ctx after copying data", KR(ret), K(file));
            copy_flush_info_fail = true;
          }
          flush_ctx_.record_flush_stage();
        }

        if (OB_FAIL(ret)) {
          int tmp_ret = OB_SUCCESS;
          ObTmpFileBatchFlushContext::ObTmpFileFlushFailRecord record(is_meta, ObTmpFileHandle(&file));
          if (OB_TMP_FAIL(flush_failed_array.push_back(record))) {
            // array is pre-allocated to MAX_COPY_FAIL_COUNT,
            // file could not be flush afterwards if push_back failed
            LOG_ERROR("fail to push back flush failed array", KR(tmp_ret), K(file.get_fd()));
          } else if (!flush_task.get_is_fast_flush_tree()
                      && flush_failed_array.size() >= ObTmpFileBatchFlushContext::MAX_COPY_FAIL_COUNT - 8) {
            fail_too_many = true;
            LOG_WARN("inner_fill_block_buf_ fail too many times", K(file.get_fd()), K(flush_failed_array.size()));
          } else if (flush_task.get_is_fast_flush_tree()
                      && flush_failed_array.size() >= ObTmpFileBatchFlushContext::MAX_COPY_FAIL_COUNT) {
            fail_too_many = true;
            LOG_WARN("inner_fill_block_buf_ fail too many times", K(file.get_fd()), K(flush_failed_array.size()));
            flush_ctx_.set_fail_too_many(true); // set this flag to exit flush
          }

          if (flush_infos.size() > origin_info_cnt) {
            flush_infos.pop_back();
          }
          if (!file_flush_ctx.data_ctx_.is_valid() && !file_flush_ctx.meta_ctx_.is_valid()) {
            // clear pre-created files to ensure that file_ctx_hash_ only records files that were able to send IO
            if (OB_TMP_FAIL(flush_ctx_.get_file_ctx_hash().erase_refactored(file.get_fd()))) {
              if (OB_HASH_NOT_EXIST != tmp_ret) {
                STORAGE_LOG(ERROR, "fail to erase file ctx from hash", KR(tmp_ret), K(file.get_fd()));
              }
            }
          }

          if (!copy_flush_info_fail) {
            STORAGE_LOG(WARN, "inner fill block buffer fail, try next file", KR(ret), K(file.get_fd()));
            ret = OB_SUCCESS; // ignore error code if fail before copying data
          }
        }
      }
    }
    file_handle.reset();
  } // end while

  return ret;
}

void ObTmpFileFlushManager::UpdateFlushCtx::operator() (hash::HashMapPair<int64_t, ObTmpFileSingleFlushContext> &pair)
{
  int64_t fd = pair.first;
  ObTmpFileDataFlushContext &data_ctx = pair.second.data_ctx_;
  ObTmpFileTreeFlushContext &meta_ctx = pair.second.meta_ctx_;
  if (input_data_ctx_.is_valid()) {
    data_ctx = input_data_ctx_;
  }
  if (input_meta_ctx_.is_valid()) {
    meta_ctx = input_meta_ctx_;
  }
  STORAGE_LOG(DEBUG, "UpdateFlushCtx after fill block data", K(fd), K(data_ctx), K(meta_ctx));
}

int ObTmpFileFlushManager::get_or_create_file_in_ctx_(const int64_t fd, ObTmpFileSingleFlushContext &file_flush_ctx)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(flush_ctx_.get_file_ctx_hash().get_refactored(fd, file_flush_ctx))) {
    if (OB_HASH_NOT_EXIST == ret) {
      ret = OB_SUCCESS;
      if (OB_FAIL(flush_ctx_.get_file_ctx_hash().set_refactored(fd, file_flush_ctx))) {
        STORAGE_LOG(WARN, "fail to insert file flush context", KR(ret), K(fd));
      }
    } else {
      STORAGE_LOG(WARN, "fail to get file flush context", KR(ret), K(fd));
    }
  }
  return ret;
}

// iterate all file to insert data items into its meta tree
int ObTmpFileFlushManager::insert_items_into_meta_tree_(ObTmpFileFlushTask &flush_task,
                                                        const int64_t logic_block_index)
{
  int ret = OB_SUCCESS;
  ObArray<ObTmpFileFlushInfo> &flush_infos = flush_task.get_flush_infos();
  for (int64_t i = 0; OB_SUCC(ret) && i < flush_infos.count(); ++i) {
    ObTmpFileFlushInfo &flush_info = flush_infos.at(i);
    ObSharedNothingTmpFile *file = flush_info.file_handle_.get();
    if (OB_ISNULL(file)) {
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "file is nullptr", KR(ret), K(flush_info));
    } else {
      if (flush_info.has_data() && !flush_info.insert_meta_tree_done_) {
        if (OB_FAIL(file->insert_meta_tree_item(flush_info, logic_block_index))) {
          STORAGE_LOG(WARN, "fail to insert meta tree item", KR(ret), K(flush_info), K(logic_block_index), KP(&flush_task));
          // flushing data pages may generate new meta pages. ff there is not enough space allocated for the meta pages,
          // it will result in the failure of the current data flushing. therefore, we need to evict some pages to free up space
          if (OB_ALLOCATE_TMP_FILE_PAGE_FAILED == ret) {
            if (OB_FAIL(evict_pages_and_retry_insert_(flush_task, flush_info, logic_block_index))) {
              STORAGE_LOG(WARN, "fail to evict pages and retry insert meta tree item",
                          KR(ret), K(flush_info), K(logic_block_index));
            }
          }
        } else {
          flush_info.insert_meta_tree_done_ = true;
        }
      }
    }
  }
  return ret;
}

// If eviction or insertion fails, retry the operation on the next round
int ObTmpFileFlushManager::evict_pages_and_retry_insert_(ObTmpFileFlushTask &flush_task,
                                                         ObTmpFileFlushInfo &flush_info,
                                                         const int64_t logic_block_index)
{
  int ret = OB_SUCCESS;
  int64_t expect_evict_page = FAST_FLUSH_TREE_PAGE_NUM;
  int64_t actual_evict_page = 0;
  ObSharedNothingTmpFile *file = flush_info.file_handle_.get();
  if (OB_ISNULL(file)) {
    ret = OB_ERR_UNEXPECTED;
    STORAGE_LOG(WARN, "file ptr is null", KR(ret), K(flush_task), K(logic_block_index));
  } else if (OB_FAIL(evict_mgr_.evict(expect_evict_page, actual_evict_page))) {
    STORAGE_LOG(WARN, "fail to evict meta pages while flushing data pages",
                KR(ret), K(flush_task), K(logic_block_index));
  } else if (OB_FAIL(file->insert_meta_tree_item(flush_info, logic_block_index))) {
    STORAGE_LOG(WARN, "fail to insert meta tree item again, retry later",
                KR(ret), K(flush_info), K(logic_block_index));
  } else {
    flush_info.insert_meta_tree_done_ = true;
  }
  return ret;
}

int ObTmpFileFlushManager::drive_flush_task_prepare_(ObTmpFileFlushTask &flush_task,
                                                    const FlushState state,
                                                    FlushState &next_state)
{
  int ret = OB_SUCCESS;
  next_state = state;
  switch (state) {
    case FlushState::TFFT_CREATE_BLOCK_INDEX:
      if (OB_FAIL(handle_create_block_index_(flush_task, next_state))) {
        STORAGE_LOG(WARN, "fail to handle flush task create block index", KR(ret), K(flush_task));
      }
      break;
    case FlushState::TFFT_FILL_BLOCK_BUF:
      if (OB_FAIL(handle_fill_block_buf_(flush_task, next_state))) {
        STORAGE_LOG(WARN, "fail to handle flush task fill block", KR(ret), K(flush_task));
      }
      break;
    case FlushState::TFFT_INSERT_META_TREE:
      if (OB_FAIL(handle_insert_meta_tree_(flush_task, next_state))) {
        STORAGE_LOG(WARN, "fail to handle flush task insert meta tree", KR(ret), K(flush_task));
      }
      break;
    case FlushState::TFFT_ASYNC_WRITE:
      if (OB_FAIL(handle_async_write_(flush_task, next_state))) {
        STORAGE_LOG(WARN, "fail to handle flush task async write", KR(ret), K(flush_task));
      }
      break;
    default:
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "unexpected state in drive_flush_task_prepare_", K(state));
      break;
  }
  return ret;
}

void ObTmpFileFlushManager::try_remove_unused_flush_info_(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;

  ObArray<ObTmpFileFlushInfo> &flush_infos = flush_task.get_flush_infos();
  for (int64_t i = 0; OB_SUCC(ret) && i >= 0 && i < flush_infos.count(); ++i) {
    ObTmpFileFlushInfo &flush_info = flush_infos.at(i);
    ObSharedNothingTmpFile *file = flush_info.file_handle_.get();
    if (OB_ISNULL(file)) {
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "file is nullptr", KR(ret), K(flush_info));
    } else if (file->is_deleting()) {
      STORAGE_LOG(INFO, "the file is deleting, abort this flush info",
          KR(ret), K(flush_info), K(flush_task));
      flush_info.reset();
      flush_infos.remove(i);
      --i;
    }
  }
}

int ObTmpFileFlushManager::drive_flush_task_retry_(
    ObTmpFileFlushTask &flush_task,
    const FlushState state,
    FlushState &next_state)
{
  int ret = OB_SUCCESS;
  next_state = state;
  switch (state) {
    case FlushState::TFFT_INSERT_META_TREE:
      if (OB_FAIL(handle_insert_meta_tree_(flush_task, next_state))) {
        STORAGE_LOG(WARN, "fail to handle flush task insert meta tree", KR(ret), K(flush_task));
      }
      break;
    case FlushState::TFFT_ASYNC_WRITE:
      try_remove_unused_flush_info_(flush_task);
      if (0 == flush_task.get_flush_infos().count()) {
        STORAGE_LOG(INFO, "all flush info is aborted", KR(ret), K(flush_task));
        next_state = FlushState::TFFT_ABORT;
      } else if (OB_FAIL(handle_async_write_(flush_task, next_state))) {
        STORAGE_LOG(WARN, "fail to handle flush task async write", KR(ret), K(flush_task));
      }
      break;
    default:
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "unexpected state in drive_flush_task_retry_", K(state), K(flush_task));
      break;
  }
  return ret;
}

int ObTmpFileFlushManager::drive_flush_task_wait_to_finish_(ObTmpFileFlushTask &flush_task, FlushState &next_state)
{
  int ret = OB_SUCCESS;
  ObTmpFileFlushTask::ObTmpFileFlushTaskState state = flush_task.get_state();
  switch (state) {
    case FlushState::TFFT_WAIT:
      if (OB_FAIL(handle_wait_(flush_task, next_state))) {
        STORAGE_LOG(WARN, "fail to handle wait", KR(ret), K(flush_task));
      }
      break;
    default:
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "unexpected state in drive_flush_task_wait_to_finish_", K(state));
      break;
  }
  return ret;
}

int ObTmpFileFlushManager::retry(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;

  FlushState state = flush_task.get_state();
  FlushState next_state = state;
  do {
    next_state = state = flush_task.get_state();
    if (OB_FAIL(drive_flush_task_retry_(flush_task, state, next_state))) {
      STORAGE_LOG(WARN, "fail to drive flush state machine", KR(ret), K(flush_task));
    } else if (flush_task.get_state() >= next_state) {
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(WARN, "unexpected flush state after drive task succ", KR(ret), K(state), K(flush_task));
    } else if (OB_FAIL(advance_status_(flush_task, next_state))) {
      STORAGE_LOG(WARN, "fail to advance status", KR(ret), K(state), K(next_state), K(flush_task));
    }
  } while (OB_SUCC(ret) && FlushState::TFFT_WAIT != next_state && FlushState::TFFT_ABORT != next_state);

  if (!flush_task.get_recorded_as_prepare_finished()) {
    if (flush_task.get_flush_seq() != flush_ctx_.get_flush_sequence()) {
      ret = OB_ERR_UNEXPECTED;
      STORAGE_LOG(ERROR, "flush_seq in flush_task is not equal to flush_seq in flush_ctx",
          KR(ret), K(flush_task), K(flush_ctx_));
    } else {
      bool recorded_as_prepare_finished = false;
      flush_ctx_.try_update_prepare_finished_cnt(flush_task, recorded_as_prepare_finished);
      if (recorded_as_prepare_finished) {
        flush_task.mark_recorded_as_prepare_finished();
        if (flush_ctx_.can_clear_flush_ctx()) {
          flush_ctx_.clear_flush_ctx(flush_priority_mgr_);
        }
      }
    }
  }
  return ret;
}

int ObTmpFileFlushManager::io_finished(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;
  FlushState next_state = FlushState::TFFT_INITED;
  if (OB_FAIL(drive_flush_task_wait_to_finish_(flush_task, next_state))) {
    STORAGE_LOG(WARN, "fail to drive flush state machine to FINISHED", KR(ret), K(flush_task));
  } else if (flush_task.get_state() < next_state && OB_FAIL(advance_status_(flush_task, next_state))) {
    // if the task encounters an IO error, its status will silently revert to TFFT_ASYNC_WRITE; do not verify status here.
    STORAGE_LOG(WARN, "fail to advance status", KR(ret), K(flush_task.get_state()), K(next_state));
  }
  return ret;
}

int ObTmpFileFlushManager::update_file_meta_after_flush(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;
  if (FlushState::TFFT_FINISH != flush_task.get_state()) {
    ret = OB_ERR_UNEXPECTED;
    STORAGE_LOG(WARN, "unexpected flush state after drive task succ", KR(ret), K(flush_task));
  } else if (OB_FAIL(handle_finish_(flush_task))) {
    STORAGE_LOG(WARN, "fail to update file meta after flush", KR(ret), K(flush_task));
  }
  return ret;
}

int ObTmpFileFlushManager::advance_status_(ObTmpFileFlushTask &flush_task, const FlushState &state)
{
  int ret = OB_SUCCESS;
  if (flush_task.get_state() >= state) {
    ret = OB_INVALID_ARGUMENT;
    STORAGE_LOG(WARN, "unexpected state in advance_status_", K(state), K(flush_task));
  } else {
    flush_task.set_state(state);
    STORAGE_LOG(DEBUG, "advance flush task status succ", K(state), K(flush_task));
  }
  return ret;
}

int ObTmpFileFlushManager::handle_alloc_flush_task_(const bool fast_flush_meta, ObTmpFileFlushTask *&flush_task)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(alloc_flush_task(flush_task))) {
    STORAGE_LOG(WARN, "fail to alloc flush callback", KR(ret));
  } else if (OB_ISNULL(flush_task)) {
    ret = OB_ERR_UNEXPECTED;
    STORAGE_LOG(WARN, "flush cb is null", KR(ret));
  } else {
    flush_task->set_state(FlushState::TFFT_CREATE_BLOCK_INDEX);
    flush_task->set_create_ts(ObTimeUtil::current_time());
    flush_task->set_flush_seq(flush_ctx_.get_flush_sequence());
    flush_task->set_is_fast_flush_tree(fast_flush_meta);
  }
  return ret;
}

int ObTmpFileFlushManager::handle_create_block_index_(ObTmpFileFlushTask &flush_task, FlushState &next_state)
{
  int ret = OB_SUCCESS;
  int64_t block_index = ObTmpFileGlobal::INVALID_TMP_FILE_BLOCK_INDEX;
  ObTmpFileBlockHandle tmp_file_block_handle;
  // occupy the whole tmp file block because we don't know how many pages we will use before TFFT_FILL_BLOCK_BUF,
  // and we still create block first because copy meta tree pages needs to provide tmp file block's block_index,
  // unused pages will be released after TFFT_FILL_BLOCK_BUF succ
  if (OB_FAIL(tmp_file_block_mgr_.create_tmp_file_block(0/*begin_page_id*/,
                                                        ObTmpFileGlobal::BLOCK_PAGE_NUMS,
                                                        block_index))) {
    STORAGE_LOG(WARN, "fail to create tmp file block", KR(ret), K(flush_task));
  } else if (OB_FAIL(tmp_file_block_mgr_.get_tmp_file_block_handle(block_index, tmp_file_block_handle))) {
    // keep a tmp file block handle in flush task to prevent rollback operations
    // caused by "append writes last not full page during the flush process" release tmp file block.
    // for example, a tmp file block will be unexpected released if a flush task
    // only contains "not full pages" and all there pages are rollback by append writes
    STORAGE_LOG(WARN, "fail to get tmp file block handle", KR(ret), K(block_index), K(flush_task));
  } else {
    flush_task.set_tmp_file_block_handle(tmp_file_block_handle);
    flush_task.set_block_index(block_index);
    next_state = FlushState::TFFT_FILL_BLOCK_BUF;
  }
  return ret;
}

int ObTmpFileFlushManager::handle_fill_block_buf_(ObTmpFileFlushTask &flush_task, FlushState &next_state)
{
  int ret = OB_SUCCESS;
  int tmp_ret = OB_SUCCESS;
  if (OB_FAIL(flush_task.prealloc_block_buf())) {
    STORAGE_LOG(WARN, "fail to prealloc block buf", KR(ret), K(flush_task));
  } else if (flush_task.get_is_fast_flush_tree()) { // skip flush level, copy meta tree pages directly
    if (OB_FAIL(fast_fill_block_buf_with_meta_(flush_task))) {
      STORAGE_LOG(WARN, "fail to fill block buffer with meta", KR(ret), K(flush_task));
    }
  } else {
    if (OB_FAIL(fill_block_buf_(flush_task))) {
      if (OB_ITER_END != ret) {
        STORAGE_LOG(WARN, "fail to fill block buf", KR(ret), K(flush_task));
      }
    } else if (0 == flush_task.get_data_length()) {
      ret = OB_ITER_END;
    }
  }

  if (OB_FAIL(ret)){
    STORAGE_LOG(WARN, "fail to fill block buf, skip release page", KR(ret));
  } else if (OB_UNLIKELY(flush_task.get_flush_infos().empty())) {
    ret = OB_ERR_UNEXPECTED;
    STORAGE_LOG(ERROR, "flush infos is empty", KR(ret), K(flush_task));
  } else {
    for (int64_t i = 0; OB_SUCC(ret) && i < flush_task.get_flush_infos().count(); ++i) {
      if (OB_UNLIKELY(flush_task.get_flush_infos().at(i).has_data() &&
                      flush_task.get_flush_infos().at(i).has_meta())) {
        ret = OB_ERR_UNEXPECTED;
        STORAGE_LOG(ERROR, "flush info has both data and meta", KR(ret), K(flush_task));
      } else if (OB_UNLIKELY((flush_task.get_flush_infos().at(0).has_data() !=
                              flush_task.get_flush_infos().at(i).has_data()) ||
                             (flush_task.get_flush_infos().at(0).has_meta() !=
                              flush_task.get_flush_infos().at(i).has_meta()))) {
        ret = OB_ERR_UNEXPECTED;
        STORAGE_LOG(ERROR, "flush infos mixed storage meta and data pages", KR(ret), K(flush_task));
      }
    }
  }

  if (OB_SUCC(ret)) {
    const bool is_whole_data_page = flush_task.get_flush_infos().at(0).has_data();
    if (is_whole_data_page &&
        OB_TMP_FAIL(ObTmpBlockCache::get_instance().put_block(flush_task.get_inst_handle(),
                                                              flush_task.get_kvpair(),
                                                              flush_task.get_block_handle()))) {
      STORAGE_LOG(WARN, "fail to put block into block cache", KR(tmp_ret), K(flush_task));
    }

    int64_t used_page_num = flush_task.get_total_page_num();
    int64_t unused_page_id = used_page_num;
    int64_t unused_page_num = ObTmpFileGlobal::BLOCK_PAGE_NUMS - used_page_num;
    int64_t block_index = flush_task.get_block_index();
    bool need_release_page = unused_page_num > 0;

    if (OB_FAIL(tmp_file_block_mgr_.write_back_start(block_index))) {
      STORAGE_LOG(ERROR, "fail to notify tmp file block write back start", KR(ret), K(block_index));
    } else if (need_release_page && OB_FAIL(tmp_file_block_mgr_.release_tmp_file_page(
                                            block_index, unused_page_id, unused_page_num))) {
      STORAGE_LOG(ERROR, "fail to release tmp file page",
          KR(ret), K(unused_page_id), K(unused_page_num), K(used_page_num), K(block_index), K(flush_task));
    } else {
      next_state = FlushState::TFFT_INSERT_META_TREE;
    }
  }

  return ret;
}

int ObTmpFileFlushManager::handle_insert_meta_tree_(ObTmpFileFlushTask &flush_task, FlushState &next_state)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(insert_items_into_meta_tree_(flush_task, flush_task.get_block_index()))) {
    STORAGE_LOG(WARN, "fail to insert meta tree", KR(ret), K(flush_task));
  } else {
    next_state = FlushState::TFFT_ASYNC_WRITE;
  }
  return ret;
}

int ObTmpFileFlushManager::handle_async_write_(ObTmpFileFlushTask &flush_task, FlushState &next_state)
{
  int ret = OB_SUCCESS;
  if (OB_FAIL(flush_task.write_one_block())) {
    STORAGE_LOG(WARN, "fail to async write blocks", KR(ret), K(flush_task));
  } else {
    next_state = FlushState::TFFT_WAIT;
  }
  return ret;
}

int ObTmpFileFlushManager::handle_wait_(ObTmpFileFlushTask &flush_task, FlushState &next_state)
{
  int ret = OB_SUCCESS;
  int task_ret_code = flush_task.atomic_get_ret_code();
  if (OB_SUCCESS != task_ret_code) {
    // rollback the status to TFFT_ASYNC_WRITE if IO failed, and re-send the I/O in the retry process.
    STORAGE_LOG(INFO, "flush_task io fail, retry it later", KR(task_ret_code), K(flush_task));
    flush_task.set_state(FlushState::TFFT_ASYNC_WRITE);
  } else if (OB_FAIL(tmp_file_block_mgr_.write_back_succ(flush_task.get_block_index(),
                                                         flush_task.get_macro_block_handle().get_macro_id()))) {
    STORAGE_LOG(WARN, "fail to notify tmp file block write back succ", KR(ret), K(flush_task));
  } else {
    next_state = FlushState::TFFT_FINISH;
  }
  return ret;
}

// Update file meta after flush task IO complete, ensures reentrancy
int ObTmpFileFlushManager::handle_finish_(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;

  if (OB_FAIL(update_meta_data_after_flush_for_files_(flush_task))) {
    STORAGE_LOG(WARN, "fail to update meta data after flush for files", KR(ret), K(flush_task));
  }  else {
    STORAGE_LOG(DEBUG, "flush task finish successfully", K(flush_task));
  }

  return ret;
}

void ObTmpFileFlushManager::ResetFlushCtxOp::operator() (hash::HashMapPair<int64_t, ObTmpFileSingleFlushContext> &pair)
{
  if (is_meta_) {
    pair.second.meta_ctx_.reset();
  } else {
    pair.second.data_ctx_.reset();
  }
}

int ObTmpFileFlushManager::update_meta_data_after_flush_for_files_(ObTmpFileFlushTask &flush_task)
{
  int ret = OB_SUCCESS;
  ObArray<ObTmpFileFlushInfo> &flush_infos = flush_task.get_flush_infos();
  for (int64_t i = 0; OB_SUCC(ret) && i < flush_infos.size(); ++i) {
    ObTmpFileFlushInfo &flush_info = flush_infos.at(i);
    ObSharedNothingTmpFile *file = flush_info.file_handle_.get();
    bool is_meta = false;
    bool reset_ctx = false;
    if (!flush_info.update_meta_data_done_) {
      if (OB_ISNULL(file)) {
        ret = OB_ERR_UNEXPECTED;
        STORAGE_LOG(WARN, "tmp file ptr is null", KR(ret), K(i), K(flush_task));
      } else if ((OB_UNLIKELY(flush_info.has_data() && flush_info.has_meta()) ||
                  OB_UNLIKELY(!flush_info.has_data() && !flush_info.has_meta()))) {
        ret = OB_ERR_UNEXPECTED; // expect one flush_info only contains one type of pages
        STORAGE_LOG(ERROR, "flush info is not valid", KR(ret), K(flush_info));
      } else if (FALSE_IT(is_meta = flush_info.has_meta())) {
      } else if (OB_FAIL(file->update_meta_after_flush(flush_info.batch_flush_idx_, is_meta, reset_ctx))){
        STORAGE_LOG(WARN, "fail to update meta data", KR(ret), K(is_meta), K(flush_info));
      } else {
        if (reset_ctx) {
          int tmp_ret = OB_SUCCESS;
          // reset data/meta flush ctx after file update meta complete to prevent
          // flush use stale flushed_page_id to copy data after the page is evicted.
          // use empty ctx here because we have inserted ctx for every file when flushing begins.
          ResetFlushCtxOp reset_op(is_meta);
          ObTmpFileSingleFlushContext empty_ctx;
          if (flush_task.get_flush_seq() == flush_ctx_.get_flush_sequence() &&
              OB_TMP_FAIL(flush_ctx_.get_file_ctx_hash().set_or_update(file->get_fd(), empty_ctx, reset_op))) {
            STORAGE_LOG(ERROR, "fail to clean file ctx from hash", KR(tmp_ret), K(file));
          }
        }
        flush_info.update_meta_data_done_ = true;
      }
    }
  }
  return ret;
}

}  // end namespace tmp_file
}  // end namespace oceanbase
