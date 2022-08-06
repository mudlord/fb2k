// This is the master foobar2000 SDK header file; it includes headers for all functionality exposed through the SDK project. #include this in your source code, never reference any of the other headers directly.

#ifndef _FOOBAR2000_H_
#define _FOOBAR2000_H_

#include "foobar2000-winver.h"

// This SDK does NOT SUPPORT targets older than API 81 / foobar2000 v2.0
// Use a 1.x series SDK if you wish to target older
#define FOOBAR2000_TARGET_VERSION 81 // 2.0

#ifdef _M_IX86
#define FOOBAR2000_TARGET_VERSION_COMPATIBLE 72
#else
// x64 & ARM64 targets
// Allow components made with new foobar2000 v1.6 SDK with x64 & ARM64 targets
#define FOOBAR2000_TARGET_VERSION_COMPATIBLE 80
#endif

// Use this to determine what foobar2000 SDK version is in use, undefined for releases older than 2018
#define FOOBAR2000_SDK_VERSION 20220617


#include "foobar2000-pfc.h"
#include "../shared/shared.h"

#ifndef NOTHROW
#ifdef _MSC_VER
#define NOTHROW __declspec(nothrow)
#else
#define NOTHROW
#endif
#endif

#define FB2KAPI /*NOTHROW*/

typedef const char * pcchar;

#include "core_api.h"
#include "service.h"
#include "service_impl.h"
#include "service_by_guid.h"
#include "service_compat.h"

#include "forward_types.h"

#include "completion_notify.h"
#include "abort_callback.h"
#include "componentversion.h"
#include "preferences_page.h"
#include "coreversion.h"
#include "filesystem.h"
#include "filesystem_transacted.h"
#include "archive.h"
#include "audio_chunk.h"
#include "mem_block_container.h"
#include "audio_postprocessor.h"
#include "playable_location.h"
#include "file_info.h"
#include "file_info_impl.h"
#include "hasher_md5.h"
#include "metadb_handle.h"
#include "metadb.h"
#include "metadb_index.h"
#include "metadb_display_field_provider.h"
#include "metadb_callbacks.h"
#include "file_info_filter.h"
#include "console.h"
#include "dsp.h"
#include "dsp_manager.h"
#include "initquit.h"
#include "event_logger.h"
#include "input.h"
#include "input_impl.h"
#include "menu.h"
#include "contextmenu.h"
#include "contextmenu_manager.h"
#include "menu_helpers.h"
#include "modeless_dialog.h"
#include "playback_control.h"
#include "play_callback.h"
#include "playlist.h"
#include "playlist_loader.h"
#include "replaygain.h"
#include "resampler.h"
#include "tag_processor.h"
#include "titleformat.h"
#include "ui.h"
#include "unpack.h"
#include "packet_decoder.h"
#include "commandline.h"
#include "genrand.h"
#include "file_operation_callback.h"
#include "library_manager.h"
#include "library_callbacks.h"
#include "config_io_callback.h"
#include "popup_message.h"
#include "app_close_blocker.h"
#include "config_object.h"
#include "threaded_process.h"
#include "input_file_type.h"
#include "main_thread_callback.h"
#include "advconfig.h"
#include "track_property.h"

#include "album_art.h"
#include "album_art_helpers.h"
#include "icon_remap.h"
#include "search_tools.h"
#include "autoplaylist.h"
#include "replaygain_scanner.h"

#include "system_time_keeper.h"
#include "http_client.h"
#include "exceptions.h"

#include "progress_meter.h"

#include "commonObjects.h"

#include "file_lock_manager.h"

#include "configStore.h"

#include "timer.h"

#endif //_FOOBAR2000_H_
