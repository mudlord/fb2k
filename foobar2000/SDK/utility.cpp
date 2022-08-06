#include "foobar2000.h"
#include "foosort.h"
#include <functional>

namespace pfc {
	/*
	Redirect PFC methods to shared.dll
	If you're getting linker multiple-definition errors on these, change build configuration of PFC from "Debug" / "Release" to "Debug FB2K" / "Release FB2K"
	*/
#ifdef _WIN32
	BOOL winFormatSystemErrorMessageHook(pfc::string_base & p_out, DWORD p_code) {
		return uFormatSystemErrorMessage(p_out, p_code);
	}
#endif
	void crashHook() {
		uBugCheck();
	}
}


// file_lock_manager.h functionality
#include "file_lock_manager.h"
namespace {
    class file_lock_interrupt_impl : public file_lock_interrupt {
    public:
        void interrupt( abort_callback & a ) { f(a); }
        std::function<void (abort_callback&)> f;
    };
}

file_lock_interrupt::ptr file_lock_interrupt::create( std::function< void (abort_callback&)> f ) {
    service_ptr_t<file_lock_interrupt_impl> i = new service_impl_t<file_lock_interrupt_impl>();
    i->f = f;
    return i;
}

// file_info_filter.h functionality
#include "file_info_filter.h"
namespace {
    class file_info_filter_lambda : public file_info_filter {
    public:
        bool apply_filter(trackRef p_track,t_filestats p_stats,file_info & p_info) override {
            return f(p_track, p_stats, p_info);
        }
        func_t f;
    };
}

file_info_filter::ptr file_info_filter::create(func_t f) {
    auto o = fb2k::service_new<file_info_filter_lambda>();
    o->f = f;
    return o;
}

// threadPool.h functionality
#include "threadPool.h"
namespace fb2k {
	void inWorkerThread(std::function<void()> f) {
		fb2k::splitTask(f);
	}
	void inCpuWorkerThread(std::function<void()> f) {
		cpuThreadPool::get()->runSingle(threadEntry::make(f));
	}
}
namespace {
	class threadEntryImpl : public fb2k::threadEntry {
	public:
		void run() override { f(); }
		std::function<void()> f;
	};
}
namespace fb2k {
	threadEntry::ptr threadEntry::make(std::function<void()> f) {
		auto ret = fb2k::service_new<threadEntryImpl>();
		ret->f = f;
		return ret;
	}

	void cpuThreadPool::runMulti_(std::function<void()> f, size_t numRuns) {
		this->runMulti(threadEntry::make(f), numRuns, true);
	}

	void cpuThreadPool::runMultiHelper(std::function<void()> f, size_t numRuns) {
		if (numRuns == 0) return;
#if FOOBAR2000_TARGET_VERSION >= 81
		get()->runMulti_(f, numRuns);
#else
		if (numRuns == 1) {
			f();
			return;
		}

		size_t numThreads = numRuns - 1;
		struct rec_t {
			pfc::thread2 trd;
			std::exception_ptr ex;
		};
		pfc::array_staticsize_t<rec_t> threads;
		threads.set_size_discard(numThreads);
		for (size_t walk = 0; walk < numThreads; ++walk) {
			threads[walk].trd.startHere([f, &threads, walk] {
				try {
					f();
				} catch (...) {
					threads[walk].ex = std::current_exception();
				}
			});
		}
		std::exception_ptr exMain;
		try {
			f();
		} catch (...) {
			exMain = std::current_exception();
		}

		for (size_t walk = 0; walk < numThreads; ++walk) {
			threads[walk].trd.waitTillDone();
		}
		if (exMain) std::rethrow_exception(exMain);
		for (size_t walk = 0; walk < numThreads; ++walk) {
			auto & ex = threads[walk].ex;
			if (ex) std::rethrow_exception(ex);
		}
#endif
	}
}


// timer.h functionality
#include "timer.h"
#include <memory>

namespace fb2k {
	void callLater(double timeAfter, std::function< void() > func) {
		auto releaseMe = std::make_shared<objRef>();
		*releaseMe = registerTimer(timeAfter, [=] {
			if (releaseMe->is_valid()) {
				releaseMe->release();
				func();
			}
		});
	}
	objRef registerTimer(double interval, std::function<void()> func) {
		return static_api_ptr_t<timerManager>()->addTimer(interval, makeCompletionNotify([func](unsigned) { func(); }));
	}
}

// autoplaylist.h functionality
#include "autoplaylist.h"

bool autoplaylist_client::supports_async_() {
	autoplaylist_client_v3::ptr v3;
	bool rv = false;
	if (v3 &= this) {
		rv = v3->supports_async();
	}
	return rv;
}

bool autoplaylist_client::supports_get_contents_() {
	autoplaylist_client_v3::ptr v3;
	bool rv = false;
	if (v3 &= this) {
		rv = v3->supports_get_contents();
	}
	return rv;
}

void autoplaylist_client_v3::filter(metadb_handle_list_cref data, bool * out) {
    filter_v2(data, nullptr, out, fb2k::noAbort);
}
bool autoplaylist_client_v3::sort(metadb_handle_list_cref p_items,t_size * p_orderbuffer) {
    return sort_v2(p_items, p_orderbuffer, fb2k::noAbort);
}


#include "noInfo.h"
namespace fb2k {
	noInfo_t noInfo;
}


// library_callbacks.h functionality
#include "library_callbacks.h"

void library_callback_dynamic::register_callback() {
	library_manager_v3::get()->register_callback(this);
}
void library_callback_dynamic::unregister_callback() {
	library_manager_v3::get()->unregister_callback(this);
}

void library_callback_v2_dynamic::register_callback() {
	library_manager_v4::get()->register_callback_v2(this);
}

void library_callback_v2_dynamic::unregister_callback() {
	library_manager_v4::get()->unregister_callback_v2(this);
}


// advconfig_impl.h functionality
#include "advconfig_impl.h"

void advconfig_entry_checkbox_impl::reset() {
	fb2k::configStore::get()->deleteConfigBool(m_varName);
}

void advconfig_entry_checkbox_impl::set_state(bool p_state) {
	fb2k::configStore::get()->setConfigBool(m_varName, p_state);
}

bool advconfig_entry_checkbox_impl::get_state_() const {
	return fb2k::configStore::get()->getConfigBool(m_varName, m_initialstate);
}

#ifdef FOOBAR2000_HAVE_CFG_VAR_LEGACY
void advconfig_entry_checkbox_impl::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort) {
	uint8_t v;
	if (p_stream->read(&v, 1, p_abort) == 1) {
		set_state(v != 0);
	}
}
#endif

pfc::string8 fb2k::advconfig_autoName(const GUID& id) {
	return pfc::format("advconfig.unnamed.", pfc::print_guid(id));
}

void advconfig_entry_string_impl::reset() {
	fb2k::configStore::get()->deleteConfigString(m_varName);
}
void advconfig_entry_string_impl::get_state(pfc::string_base& p_out) {
	p_out = fb2k::configStore::get()->getConfigString(m_varName, m_initialstate)->c_str();
}
void advconfig_entry_string_impl::set_state(const char* p_string, t_size p_length) {
	pfc::string8 asdf;
	if (p_length != SIZE_MAX) {
		asdf.set_string(p_string, p_length);
		p_string = asdf;
	}
	fb2k::configStore::get()->setConfigString(m_varName, p_string );
}

#ifdef FOOBAR2000_HAVE_CFG_VAR_LEGACY
void advconfig_entry_string_impl::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort) {
	pfc::string8_fastalloc temp;
	p_stream->read_string_raw(temp, p_abort);
	this->set_state(temp);
}
#endif

// configCache.h functionality
#include "configCache.h"

bool fb2k::configBoolCache::get() {
	std::call_once(m_init, [this] {
		auto api = fb2k::configStore::get();
		auto refresh = [this, api] { m_value = api->getConfigBool(m_var, m_def); };
		api->addPermanentNotify(m_var, refresh);
		refresh();
	});
	return m_value;
}

int64_t fb2k::configIntCache::get() {
	std::call_once(m_init, [this] {
		auto api = fb2k::configStore::get();
		auto refresh = [this, api] { m_value = api->getConfigInt(m_var, m_def); };
		api->addPermanentNotify(m_var, refresh);
		refresh();
	});
	return m_value;
}

void fb2k::configBoolCache::set(bool v) {
	m_value = v;
	fb2k::configStore::get()->setConfigBool(m_var, v);
}

void fb2k::configIntCache::set(int64_t v) {
	m_value = v;
	fb2k::configStore::get()->setConfigBool(m_var, v);
}


// cfg_var.h functionality
#include "cfg_var.h"

#ifdef FOOBAR2000_HAVE_CFG_VAR_LEGACY
void cfg_string::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort) {
	pfc::string8_fastalloc temp;
	p_stream->read_string_raw(temp, p_abort);
	this->set(temp);
}

void cfg_int::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort) {
	switch (p_sizehint) {
	case 4:
		{ int32_t v; p_stream->read_lendian_t(v, p_abort); set(v); }
		break;
	case 8:
		{ int64_t v; p_stream->read_lendian_t(v, p_abort); set(v); }
		break;
	}
}

void cfg_bool::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort) {
	uint8_t b;
	if (p_stream->read(&b, 1, p_abort) == 1) {
		this->set(b != 0);
	}
}
#endif

int64_t cfg_int::get() {
	std::call_once(m_init, [this] {
		m_val = fb2k::configStore::get()->getConfigInt(formatName(), m_initVal);
	});
	return m_val;
}

void cfg_int::set(int64_t v) {
	if (v != get()) {
		m_val = v;
		fb2k::configStore::get()->setConfigInt(formatName(), v);
	}
}

bool cfg_bool::get() {
	std::call_once(m_init, [this] {
		m_val = fb2k::configStore::get()->getConfigBool(formatName(), m_initVal);
	});
	return m_val;
}

void cfg_bool::set(bool v) {
	if (v != get()) {
		m_val = v;
		fb2k::configStore::get()->setConfigBool(formatName(), v);
	}
}

void cfg_string::set(const char* v) {
	if (strcmp(v, get()) != 0) {
		pfc::string8 obj = v;

		{
			PFC_INSYNC_WRITE(m_valueGuard);
			m_value = std::move(obj);
		}

		fb2k::configStore::get()->setConfigString(formatName(), v);
	}
}

void cfg_string::get(pfc::string_base& out) {
	std::call_once(m_init, [this] {
		pfc::string8 v = fb2k::configStore::get()->getConfigString(formatName(), m_initVal)->c_str();
		PFC_INSYNC_WRITE(m_valueGuard);
		m_value = std::move(v);
	});

	PFC_INSYNC_READ(m_valueGuard);
	out = m_value;
}

pfc::string8 cfg_string::get() {
	pfc::string8 ret; get(ret); return ret;
}


pfc::string8 cfg_var_common::formatVarName(const GUID& guid) {
	return pfc::format("cfg_var.", pfc::print_guid(guid));
}

pfc::string8 cfg_var_common::formatName() const {
	return formatVarName(this->m_guid);
}

fb2k::memBlockRef cfg_blob::get() {
	std::call_once(m_init, [this] {
		auto v = fb2k::configStore::get()->getConfigBlob(formatName(), m_initVal);
		PFC_INSYNC_WRITE(m_dataGuard);
		m_data = std::move(v);
	});
	PFC_INSYNC_READ(m_dataGuard);
	return m_data;
}

void cfg_blob::set_(fb2k::memBlockRef arg) {
	{
		PFC_INSYNC_WRITE(m_dataGuard);
		m_data = arg;
	}

	auto api = fb2k::configStore::get();
	auto name = formatName();
	if (arg.is_valid()) api->setConfigBlob(name, arg);
	else api->deleteConfigBlob(name);
}

void cfg_blob::set(fb2k::memBlockRef arg) {
	auto old = get();
	if (!fb2k::memBlock::equals(old, arg)) {
		set_(arg);
	}
}

void cfg_blob::set(const void* ptr, size_t size) {
	set(fb2k::makeMemBlock(ptr, size));
}

#ifdef FOOBAR2000_HAVE_CFG_VAR_LEGACY
void cfg_blob::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort) {
	pfc::mem_block block;
	block.resize(p_sizehint);
	p_stream->read_object(block.ptr(), p_sizehint, p_abort);
	set(fb2k::memBlock::blockWithData(std::move(block)));
}
#endif

double cfg_float::get() {
	std::call_once(m_init, [this] {
		m_val = fb2k::configStore::get()->getConfigFloat(formatName(), m_initVal);
	});
	return m_val;
}

void cfg_float::set(double v) {
	if (v != get()) {
		m_val = v;
		fb2k::configStore::get()->setConfigFloat(formatName(), v);
	}
}

#ifdef FOOBAR2000_HAVE_CFG_VAR_LEGACY
void cfg_float::set_data_raw(stream_reader* p_stream, t_size p_sizehint, abort_callback& p_abort) {
	switch (p_sizehint) {
	case 4:
	{ float v; if (p_stream->read(&v, 4, p_abort) == 4) set(v); }
	break;
	case 8:
	{ double v; if (p_stream->read(&v, 8, p_abort) == 8) set(v); }
	break;
	}
}
#endif

// keyValueIO.h functionality
#include "keyValueIO.h"

int fb2k::keyValueIO::getInt( const char * name ) {
    auto str = get(name);
    if ( str.is_empty() ) return 0;
    return (int) pfc::atoi64_ex( str->c_str(), str->length() );
}

void fb2k::keyValueIO::putInt( const char * name, int val ) {
    put( name, pfc::format_int(val) );
}


// fileDialog.h functionality
#include "fileDialog.h"

namespace {
    using namespace fb2k;
    class fileDialogNotifyImpl : public fileDialogNotify {
    public:
        void dialogCancelled() {}
        void dialogOK2(arrayRef fsItems) {
            recv(fsItems);
        }

        std::function< void (arrayRef) > recv;
    };
}

fb2k::fileDialogNotify::ptr fb2k::fileDialogNotify::create( std::function<void (arrayRef) > recv ) {
    service_ptr_t<fileDialogNotifyImpl> obj = new service_impl_t< fileDialogNotifyImpl >();
    obj->recv = recv;
    return obj;
}

#include "input_file_type.h"

void fb2k::fileDialogSetup::setAudioFileTypes() {
    pfc::string8 temp;
    input_file_type::build_openfile_mask(temp);
    this->setFileTypes( temp );
}
