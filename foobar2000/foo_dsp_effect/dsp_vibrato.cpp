
#define _USE_MATH_DEFINES
#include "../helpers/foobar2000+atl.h"
#include <coreDarkMode.h>
#include "../../libPPUI/win32_utility.h"
#include "../../libPPUI/win32_op.h" // WIN32_OP()
#include "../SDK/ui_element.h"
#include "../helpers/BumpableElem.h"
#include "../../libPPUI/CDialogResizeHelper.h"
#include "resource.h"
#include "dsp_guids.h"

#include <vector>
namespace {

	class CEditMod : public CWindowImpl<CEditMod, CEdit >
	{
	public:
		BEGIN_MSG_MAP(CEditMod)
			MESSAGE_HANDLER(WM_CHAR, OnChar)
		END_MSG_MAP()

		CEditMod(HWND hWnd = NULL) { }
		LRESULT OnChar(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			switch (wParam)
			{
			case '\r': //Carriage return
				::PostMessage(m_parent, WM_USER, 0x1988, 0L);
				return 0;
				break;
			}
			return DefWindowProc(uMsg, wParam, lParam);
		}
		void AttachToDlgItem(HWND parent)
		{
			m_parent = parent;
		}
	private:
		UINT m_dlgItem;
		HWND m_parent;
	};

	const float BASE_DELAY_SEC = 0.002; // 2 ms
	const float VIBRATO_FREQUENCY_DEFAULT_HZ = 2;
	const float VIBRATO_FREQUENCY_MAX_HZ = 14;
	const float VIBRATO_DEPTH_DEFAULT_PERCENT = 50;

	__forceinline audio_sample getSampleHermite4p3o(audio_sample x, audio_sample *y)
	{
		static audio_sample c0, c1, c2, c3;
		// 4-point, 3rd-order Hermite (x-form)
		c0 = y[1];
		c1 = (1.0 / 2.0)*(y[2] - y[0]);
		c2 = (y[0] - (5.0 / 2.0)*y[1]) + (2.0*y[2] - (1.0 / 2.0)*y[3]);
		c3 = (1.0 / 2.0)*(y[3] - y[0]) + (3.0 / 2.0)*(y[1] - y[2]);
		return ((c3*x + c2)*x + c1)*x + c0;
	}

	class Vibrato
	{
	private:
		float freq;
		float samplerate;
		int phase;
		float depth;
		static const int additionalDelay = 3;
		audio_sample*  buffer;
		audio_sample *table;
		int writeIndex;
		int size;
	public:
		Vibrato()
		{
			buffer = NULL;
		}
		~Vibrato()
		{
			delete[]buffer;
			buffer = NULL;
		}
		void init(float freq, float depth, int samplerate)
		{
			size = BASE_DELAY_SEC * samplerate * 2;
			buffer = new audio_sample[size + additionalDelay];
			memset(buffer, 0, (size + additionalDelay) * sizeof(audio_sample));
			this->samplerate = samplerate;
			this->freq = freq;
			this->depth = depth;
			phase = 0;
			writeIndex = 0;

		}
		audio_sample Process(audio_sample in)
		{
			audio_sample M = freq / samplerate;
			int maxphase = samplerate / freq;
			audio_sample lfoValue = sin(M * 2. * M_PI * phase++);
			phase = phase % maxphase;
			lfoValue = (lfoValue + 1) * 1.; // transform from [-1; 1] to [0; 1]
			int maxDelay = BASE_DELAY_SEC * samplerate;
			audio_sample delay = lfoValue * depth * maxDelay;
			delay += additionalDelay;
			audio_sample fReadIndex = writeIndex - 1 - delay;
			while (fReadIndex < 0)fReadIndex += size;
			while (fReadIndex >= size)fReadIndex -= size;
			int iPart = (int)fReadIndex; // integer part of the delay
			audio_sample fPart = fReadIndex - iPart; // fractional part of the delay
			audio_sample value = getSampleHermite4p3o(fPart, &(buffer[iPart]));
			buffer[writeIndex] = in;
			if (writeIndex < additionalDelay) {
				buffer[size + writeIndex] = in;
			}
			writeIndex++;
			if (writeIndex == size) {
				writeIndex = 0;
			}
			return value;
		}
	};
	static void RunConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback);
	class dsp_vibrato : public dsp_impl_base
	{
		int m_rate, m_ch, m_ch_mask;
		float freq, depth;
		bool enabled;
		pfc::array_t<Vibrato> m_buffers;
	public:
		dsp_vibrato(dsp_preset const & in) :m_rate(0), m_ch(0), m_ch_mask(0) {
			// Mark buffer as empty.
			freq = 2.0;
			depth = 0.5;
			enabled = true;
			parse_preset(freq, depth, enabled, in);
		}

		// Every DSP type is identified by a GUID.
		static GUID g_get_guid() {
			return guid_vibrato;
		}

		// We also need a name, so the user can identify the DSP.
		// The name we use here does not describe what the DSP does,
		// so it would be a bad name. We can excuse this, because it
		// doesn't do anything useful anyway.
		static void g_get_name(pfc::string_base & p_out) {
			p_out = "Vibrato";
		}

		virtual void on_endoftrack(abort_callback & p_abort) {
			// This method is called when a track ends.
			// We need to do the same thing as flush(), so we just call it.
		}

		virtual void on_endofplayback(abort_callback & p_abort) {
			// This method is called on end of playback instead of flush().
			// We need to do the same thing as flush(), so we just call it.
		}

		// The framework feeds input to our DSP using this method.
		// Each chunk contains a number of samples with the same
		// stream characteristics, i.e. same sample rate, channel count
		// and channel configuration.
		virtual bool on_chunk(audio_chunk * chunk, abort_callback & p_abort) {
			if (chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask)
			{
				m_rate = chunk->get_srate();
				m_ch = chunk->get_channels();
				m_ch_mask = chunk->get_channel_config();
				m_buffers.set_count(0);
				m_buffers.set_count(m_ch);
				for (unsigned i = 0; i < m_ch; i++)
				{
					Vibrato & e = m_buffers[i];
					e.init(freq, depth, m_rate);
				}
			}

			for (unsigned i = 0; i < m_ch; i++)
			{
				Vibrato & e = m_buffers[i];
				audio_sample * data = chunk->get_data() + i;
				for (unsigned j = 0, k = chunk->get_sample_count(); j < k; j++)
				{
					*data = e.Process(*data);
					data += m_ch;
				}
			}
			return true;
		}

		virtual void flush() {
			m_buffers.set_count(0);
			m_rate = 0;
			m_ch = 0;
			m_ch_mask = 0;
		}

		virtual double get_latency() {
			// If the buffered chunk is valid, return its length.
			// Otherwise return 0.
			return 0.0;
		}

		virtual bool need_track_change_mark() {
			// Return true if you need to know exactly when a new track starts.
			// Beware that this may break gapless playback, as at least all the
			// DSPs before yours have to be flushed.
			// To picture this, consider the case of a reverb DSP which outputs
			// the sum of the input signal and a delayed copy of the input signal.
			// In the case of a single track:

			// Input signal:   01234567
			// Delayed signal:   01234567

			// For two consecutive tracks with the same stream characteristics:

			// Input signal:   01234567abcdefgh
			// Delayed signal:   01234567abcdefgh

			// If the DSP chain contains a DSP that requires a track change mark,
			// the chain will be flushed between the two tracks:

			// Input signal:   01234567  abcdefgh
			// Delayed signal:   01234567  abcdefgh
			return false;
		}
		static bool g_get_default_preset(dsp_preset & p_out)
		{
			make_preset(5., 0.5, true, p_out);
			return true;
		}
		static void g_show_config_popup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
		{
			::RunConfigPopup(p_data, p_parent, p_callback);
		}
		static bool g_have_config_popup() { return true; }
		static void make_preset(float freq, float depth, bool enabled, dsp_preset & out)
		{
			dsp_preset_builder builder;
			builder << freq;
			builder << depth;
			builder << enabled;
			builder.finish(g_get_guid(), out);
		}
		static void parse_preset(float & freq, float & depth, bool & enabled, const dsp_preset & in)
		{
			try
			{
				dsp_preset_parser parser(in);
				parser >> freq;
				parser >> depth;
				parser >> enabled;
			}
			catch (exception_io_data) { freq = 2.; depth = 0.5; enabled = true; }
		}
	};

	// We need a service factory to make the DSP known to the system.
	// DSPs use special service factories that implement a static dsp_entry
	// that provides information about the DSP. The static methods in our
	// DSP class our used to provide the implementation of this entry class.
	// The entry is used to instantiate an instance of our DSP when it is needed.
	// We use the "nopreset" version of the DSP factory which blanks out
	// preset support.
	// Note that there can be multiple instances of a DSP which are used in
	// different threads.
	static dsp_factory_t<dsp_vibrato> foo_dsp_tutorial_nopreset;

	class CMyDSPPopupVibrato : public CDialogImpl<CMyDSPPopupVibrato>
	{
	public:
		CMyDSPPopupVibrato(const dsp_preset & initData, dsp_preset_edit_callback & callback) : m_initData(initData), m_callback(callback) { }
		enum { IDD = IDD_TREMELO };
		enum
		{
			FreqMin = 200,
			FreqMax = 20000,
			FreqRangeTotal = FreqMax - FreqMin,
			depthmin = 0,
			depthmax = 100,
		};

		BEGIN_MSG_MAP(CMyDSPPopup)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
			COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
			MSG_WM_HSCROLL(OnHScroll)
			MESSAGE_HANDLER(WM_USER, OnEditControlChange)
			COMMAND_HANDLER_EX(IDC_RESETCHR6, BN_CLICKED, OnReset5)
		END_MSG_MAP()

	private:
		LRESULT OnEditControlChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (wParam == 0x1988)
			{
				GetEditText();
			}
			return 0;
		}

		void OnReset5(UINT, int id, CWindow)
		{
			freq = 2.; depth = 0.5;
			dsp_preset_impl preset;
			dsp_vibrato::make_preset(freq, depth, true, preset);
			m_callback.on_preset_changed(preset);
			slider_freq.SetPos((double)(100 * freq));
			slider_depth.SetPos((double)(100 * depth));
			RefreshLabel(freq, depth);
		}

		void GetEditText()
		{
			bool preset_changed = false;
			dsp_preset_impl preset;
			CString text, text2, text3, text4;
			freq_edit.GetWindowText(text);
			float freqhz = _ttof(text);
			freqhz = pfc::clip_t<t_float32>(freqhz, 2.0, 200);
			if (freq_s != text)
			{
				preset_changed = true;
				freq = freqhz;
			}
			depth_edit.GetWindowText(text2);
			float depth2 = _ttoi(text2);
			depth2 = pfc::clip_t<t_int32>(depth2,depthmin, depthmax);
			if (depth_s != text2)
			{
				preset_changed = true;
				depth = depth2 / 100.0;
			}

			if (preset_changed)
			{
				dsp_preset_impl preset;
				dsp_vibrato::make_preset(freq, depth, true, preset);
				m_callback.on_preset_changed(preset);
				slider_freq.SetPos((double)(100 * freq));
				slider_depth.SetPos((double)(100 * depth));
				RefreshLabel(freq, depth);
			}
		}


		fb2k::CCoreDarkModeHooks m_hooks;
		BOOL OnInitDialog(CWindow, LPARAM)
		{
			slider_freq = GetDlgItem(IDC_TREMELOFREQ);
			slider_freq.SetRange(FreqMin, FreqMax);
			slider_depth = GetDlgItem(IDC_TREMELODEPTH);
			slider_depth.SetRange(0, depthmax);


			freq_edit.AttachToDlgItem(m_hWnd);
			freq_edit.SubclassWindow(GetDlgItem(IDC_EDITTREMFREQ));
			depth_edit.AttachToDlgItem(m_hWnd);
			depth_edit.SubclassWindow(GetDlgItem(IDC_EDITTREMDEPTH));
			{
				bool enabled;
				dsp_vibrato::parse_preset(freq, depth, enabled, m_initData);
				slider_freq.SetPos((double)(100 * freq));
				slider_depth.SetPos((double)(100 * depth));
				RefreshLabel(freq, depth);
			}
			m_hooks.AddDialogWithControls(m_hWnd);

			return TRUE;
		}

		void OnButton(UINT, int id, CWindow)
		{
			EndDialog(id);
		}

		void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
		{

			freq = slider_freq.GetPos() / 100.0;
			depth = slider_depth.GetPos() / 100.0;
			if (LOWORD(nSBCode) != SB_THUMBTRACK)
			{
				dsp_preset_impl preset;
				dsp_vibrato::make_preset(freq, depth, true, preset);
				m_callback.on_preset_changed(preset);
			}
			RefreshLabel(freq, depth);

		}


		void RefreshLabel(float freq, float depth)
		{
			CString sWindowText;
			pfc::string_formatter msg;
			msg << pfc::format_float(freq, 0, 2);
			sWindowText = msg.c_str();
			freq_s = sWindowText;
			freq_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(depth * 100);
			sWindowText = msg.c_str();
			depth_s = sWindowText;
			depth_edit.SetWindowText(sWindowText);
		}

		const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
		dsp_preset_edit_callback & m_callback;
		float freq; //0.1 - 4.0
		float depth;  //0 - 360
		CTrackBarCtrl slider_freq, slider_depth;
		CEditMod freq_edit, depth_edit;
		CString freq_s, depth_s;
	};

	static void RunConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
	{
		CMyDSPPopupVibrato popup(p_data, p_callback);
		if (popup.DoModal(p_parent) != IDOK) p_callback.on_preset_changed(p_data);
	}

	// {1DC17CA0-0023-4266-AD59-691D566AC291}
	static const GUID guid_choruselem =
	{ 0x715a5d0c, 0x5eb2, 0x49c2,{ 0xa2, 0xfb, 0xc7, 0xb2, 0x30, 0x8, 0xe2, 0x50 } };

	static const CDialogResizeHelper::Param chorus_uiresize[] = {
		// Dialog resize handling matrix, defines how the controls scale with the dialog
		//			 L T R B
		{IDC_STATIC1, 0,0,0,0  },
		{IDC_STATIC2,    0,0,0,0 },
		{IDC_TREMELOENABLED,    0,0,0,0  },
		{IDC_EDITTREMFREQUI, 0,0,0,0 },
		{IDC_EDITTREMDEPTHUI,  0,0,0,0 },
	{IDC_RESETCHR5,0,0,0,0 },
	{IDC_TREMELOFREQ1, 0,0,1,0},
	{IDC_TREMELODEPTH1, 0,0,1,0},
	// current position of a control is determined by initial_position + factor * (current_dialog_size - initial_dialog_size)
	// where factor is the value from the table above
	// applied to all four values - left, top, right, bottom
	// 0,0,0,0 means that a control doesn't react to dialog resizing (aligned to top+left, no resize)
	// 1,1,1,1 means that the control is aligned to bottom+right but doesn't resize
	// 0,0,1,0 means that the control disregards vertical resize (aligned to top) and changes its width with the dialog
	};
	static const CRect resizeMinMax(170, 70, 1000, 1000);


	class uielem_vibrato : public CDialogImpl<uielem_vibrato>, public ui_element_instance, private play_callback_impl_base {
	public:
		uielem_vibrato(ui_element_config::ptr cfg, ui_element_instance_callback::ptr cb) : m_callback(cb), m_resizer(chorus_uiresize, resizeMinMax) {
			freq = 2.0;
			depth = 0.5;
			echo_enabled = true;

		}
		enum { IDD = IDD_TREMELO1 };
		enum
		{
			FreqMin = 200,
			FreqMax = 2000,
			FreqRangeTotal = FreqMax - FreqMin,
			depthmin = 0,
			depthmax = 100,
		};
		BEGIN_MSG_MAP(uielem_vibrato)
			CHAIN_MSG_MAP_MEMBER(m_resizer)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_HANDLER_EX(IDC_TREMELOENABLED, BN_CLICKED, OnEnabledToggle)
			MSG_WM_HSCROLL(OnScroll)
			MESSAGE_HANDLER(WM_USER, OnEditControlChange)
			COMMAND_HANDLER_EX(IDC_RESETCHR5, BN_CLICKED, OnReset5)
		END_MSG_MAP()



		void initialize_window(HWND parent) { WIN32_OP(Create(parent) != NULL); }
		HWND get_wnd() { return m_hWnd; }
		void set_configuration(ui_element_config::ptr config) {
			shit = parseConfig(config);
			if (m_hWnd != NULL) {
				ApplySettings();
			}
			m_callback->on_min_max_info_change();
		}
		ui_element_config::ptr get_configuration() { return makeConfig(); }
		static GUID g_get_guid() {
			return guid_choruselem;
		}
		static void g_get_name(pfc::string_base & out) { out = "Vibrato"; }
		static ui_element_config::ptr g_get_default_configuration() {
			return makeConfig(true);
		}
		static const char * g_get_description() { return "Modifies the 'Vibrato' DSP effect."; }
		static GUID g_get_subclass() {
			return ui_element_subclass_dsp;
		}

		ui_element_min_max_info get_min_max_info() {
			ui_element_min_max_info ret;

			// Note that we play nicely with separate horizontal & vertical DPI.
			// Such configurations have not been ever seen in circulation, but nothing stops us from supporting such.
			CSize DPI = QueryScreenDPIEx(*this);

			if (DPI.cx <= 0 || DPI.cy <= 0) { // sanity
				DPI = CSize(96, 96);
			}



			ret.m_min_width = MulDiv(170, DPI.cx, 96);
			ret.m_min_height = MulDiv(70, DPI.cy, 96);
			ret.m_max_width = MulDiv(1000, DPI.cx, 96);
			ret.m_max_height = MulDiv(1000, DPI.cy, 96);

			// Deal with WS_EX_STATICEDGE and alike that we might have picked from host
			ret.adjustForWindow(*this);

			return ret;
		}

	private:
		void on_playback_starting(play_control::t_track_command p_command, bool p_paused) { ApplySettings(); }
		void on_playback_stop(play_control::t_stop_reason p_reason) { ApplySettings(); }
		void on_playback_seek(double p_time) { ApplySettings(); }
		void on_playback_pause(bool p_state) { ApplySettings(); }
		void on_playback_edited(metadb_handle_ptr p_track) {  }
		void on_playback_dynamic_info(const file_info& p_info) {  }
		void on_playback_dynamic_info_track(const file_info& p_info) { }
		void on_playback_time(double p_time) { }
		void on_volume_change(float p_new_val) { }
		void on_playback_new_track(metadb_handle_ptr p_track) {}

		void OnReset5(UINT, int id, CWindow)
		{
			freq = 2.; depth = 0.5;
			OnConfigChanged();
			SetConfig();
		}

		LRESULT OnEditControlChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (wParam == 0x1988)
			{
				GetEditText();
			}
			return 0;
		}

		void GetEditText()
		{
			bool preset_changed = false;
			dsp_preset_impl preset;
			CString text, text2, text3, text4;
			freq_edit.GetWindowText(text);
			float freqhz = _ttof(text);
			freqhz = pfc::clip_t<t_float32>(freqhz, FreqMin, FreqMax);
			if (freq_s != text)
			{
				preset_changed = true;
				freq = freqhz;
			}


			depth_edit.GetWindowText(text2);
			float depth2 = _ttoi(text2);
			depth2 = pfc::clip_t<t_int32>(depth2,0, 100);
			if (depth_s != text2)
			{
				preset_changed = true;
				depth = depth2/100.;
			}

			if (preset_changed)
			{
				OnConfigChanged();
				SetConfig();
			}
				
		}
		fb2k::CCoreDarkModeHooks m_hooks;
		void SetEchoEnabled(bool state) { m_buttonEchoEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
		bool IsEchoEnabled() { return m_buttonEchoEnabled == NULL || m_buttonEchoEnabled.GetCheck() == BST_CHECKED; }

		void EchoDisable() {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_vibrato);
		}


		void EchoEnable(float freq, float  depth, bool echo_enabled) {
			dsp_preset_impl preset;
			dsp_vibrato::make_preset(freq, depth, echo_enabled, preset);
			static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
		}

		void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
			pfc::vartoggle_t<bool> ownUpdate(m_ownEchoUpdate, true);
			if (IsEchoEnabled()) {
				GetConfig();
				dsp_preset_impl preset;
				dsp_vibrato::make_preset(freq, depth, echo_enabled, preset);
				//yes change api;
				static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
			}
			else {
				static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_vibrato);
			}

		}

		void OnScroll(UINT scrollID, int pos, CWindow window)
		{
			pfc::vartoggle_t<bool> ownUpdate(m_ownEchoUpdate, true);
			GetConfig();
			if (IsEchoEnabled())
			{
				if (LOWORD(scrollID) != SB_THUMBTRACK)
				{
					EchoEnable(freq, depth, echo_enabled);
				}
			}

		}

		void OnChange(UINT, int id, CWindow)
		{
			pfc::vartoggle_t<bool> ownUpdate(m_ownEchoUpdate, true);
			GetConfig();
			if (IsEchoEnabled())
			{

				OnConfigChanged();
			}
		}

		void DSPConfigChange(dsp_chain_config const & cfg)
		{
			if (!m_ownEchoUpdate && m_hWnd != NULL) {
				ApplySettings();
			}
		}

		//set settings if from another control
		void ApplySettings()
		{
			dsp_preset_impl preset;
			if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_vibrato, preset)) {
				SetEchoEnabled(true);
				dsp_vibrato::parse_preset(freq, depth, echo_enabled, preset);
				SetEchoEnabled(echo_enabled);
				SetConfig();
			}
			else {
				SetEchoEnabled(false);
				SetConfig();
			}
		}

		void OnConfigChanged() {
			if (IsEchoEnabled()) {
				EchoEnable(freq, depth, echo_enabled);
			}
			else {
				EchoDisable();
			}

		}


		void GetConfig()
		{
			freq = slider_freq.GetPos() / 100.0;
			depth = slider_depth.GetPos() / 100.0;
			echo_enabled = IsEchoEnabled();
			RefreshLabel(freq, depth*100);


		}

		void SetConfig()
		{
			slider_freq.SetPos((double)(100 * freq));
			slider_depth.SetPos((double)(100 * depth));
			RefreshLabel(freq, depth*100);

		}

		BOOL OnInitDialog(CWindow, LPARAM)
		{
			slider_freq = GetDlgItem(IDC_TREMELOFREQ1);
			slider_freq.SetRange(FreqMin, FreqMax);
			slider_depth = GetDlgItem(IDC_TREMELODEPTH1);
			slider_depth.SetRange(0, depthmax);
			freq_edit.AttachToDlgItem(m_hWnd);
			freq_edit.SubclassWindow(GetDlgItem(IDC_EDITTREMFREQUI));
			depth_edit.AttachToDlgItem(m_hWnd);
			depth_edit.SubclassWindow(GetDlgItem(IDC_EDITTREMDEPTHUI));
			m_buttonEchoEnabled = GetDlgItem(IDC_TREMELOENABLED);
			m_ownEchoUpdate = false;

			ApplySettings();
			m_hooks.AddDialogWithControls(m_hWnd);
			return TRUE;
		}


		void RefreshLabel(float freq, float depth)
		{
			CString sWindowText;
			pfc::string_formatter msg;
			msg << pfc::format_float(freq, 0, 1);
			sWindowText = msg.c_str();
			freq_s = sWindowText;
			freq_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(depth);
			sWindowText = msg.c_str();
			depth_s = sWindowText;
			depth_edit.SetWindowText(sWindowText);
		}

		bool echo_enabled;
		float freq; //0.1 - 4.0
		float depth;  //0 - 360
		CTrackBarCtrl slider_freq, slider_depth;
		CButton m_buttonEchoEnabled;
		CEditMod freq_edit, depth_edit;
		CString freq_s, depth_s;
		bool m_ownEchoUpdate;
		CDialogResizeHelper m_resizer;

		static uint32_t parseConfig(ui_element_config::ptr cfg) {
			return 1;
		}
		static ui_element_config::ptr makeConfig(bool init = false) {
			ui_element_config_builder out;

			if (init)
			{
				uint32_t crap = 1;
				out << crap;
			}
			else
			{
				uint32_t crap = 2;
				out << crap;
			}
			return out.finish(g_get_guid());
		}
		uint32_t shit;
	protected:
		const ui_element_instance_callback::ptr m_callback;
	};

	class myElem_t : public  ui_element_impl_withpopup< uielem_vibrato > {
		bool get_element_group(pfc::string_base & p_out)
		{
			p_out = "Effect DSP";
			return true;
		}

		bool get_menu_command_description(pfc::string_base & out) {
			out = "Opens a window for pitch modulation control.";
			return true;
		}

		bool get_popup_specs(ui_size& defSize, pfc::string_base& title)
		{
			defSize = { 170,100 };
			title = "Vibrato DSP";
			return true;
		}

	};
	static service_factory_single_t<myElem_t> g_myElemFactory;

}
