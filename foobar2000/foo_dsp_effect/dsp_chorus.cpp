#define _USE_MATH_DEFINES
#include "../helpers/foobar2000+atl.h"
#include "../../libPPUI/win32_utility.h"
#include "../../libPPUI/win32_op.h" // WIN32_OP()
#include "../helpers/atl-misc.h"// ui_element_impl
#include "../helpers/BumpableElem.h"// ui_element_impl
#include "resource.h"
#include "dsp_guids.h"

namespace {

	static double clamp_ml(double x, double upper, double lower)
	{
		return min(upper, max(x, lower));
	}

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

#define CHORUS_MAX_DELAY 4096
#define CHORUS_DELAY_MASK (CHORUS_MAX_DELAY - 1)
#define	MODF(n,i,f) ((i) = (int)(n), (f) = (n) - (double)(i))
#define ROUND(n)		((int)((double)(n)+0.5))
#define PIN(n,min,max) ((n) > (max) ? max : ((n) < (min) ? (min) : (n)))

	class Chorus
	{
	private:
		float old[CHORUS_MAX_DELAY];
		unsigned old_ptr;
		float delay_;
		float depth_;
		float delay;
		float depth;
		float rate;
		float mix_dry;
		float mix_wet;
		unsigned lfo_ptr;
		unsigned lfo_period;
		float lfo_freq;
		float drywet;

	public:
		Chorus()
		{

		}
		~Chorus()
		{
		}

		void init(float delay, float depth, float lfo_freq, float drywet, int rate)
		{
			memset(old, 0, CHORUS_MAX_DELAY * sizeof(float));
			old_ptr = 0;
			delay_ = delay / 1000.0f;
			depth_ = depth / 1000.0f;
			this->delay = delay;
			this->depth = depth;
			this->lfo_freq = lfo_freq;
			if (depth_ > delay_)
				depth_ = delay_;

			if (drywet < 0.0f)
				drywet = 0.0f;
			else if (drywet > 1.0f)
				drywet = 1.0f;

			mix_dry = 1.0f - 0.5f * drywet;
			mix_wet = 0.6f * drywet;

			lfo_period = (1.0f / lfo_freq) * rate;
			if (!lfo_period)
				lfo_period = 1;
			this->rate = rate;
			lfo_ptr = 0;

		}
		float Process(float in)
		{
			float in_smp = in;
			old[old_ptr] = in_smp;
			float delay2 = this->delay_ + depth_ * sin((2.0 * M_PI * lfo_ptr++) / lfo_period);
			delay2 *= rate;
			if (lfo_ptr >= lfo_period)
				lfo_ptr = 0;
			unsigned delay_int = (unsigned)delay2;
			if (delay_int >= CHORUS_MAX_DELAY - 1)
				delay_int = CHORUS_MAX_DELAY - 2;
			float delay_frac = delay2 - delay_int;

			float l_a = old[(old_ptr - delay_int - 0) & CHORUS_DELAY_MASK];
			float l_b = old[(old_ptr - delay_int - 1) & CHORUS_DELAY_MASK];
			/* Lerp introduces aliasing of the chorus component,
			* but doing full polyphase here is probably overkill. */
			float chorus_l = l_a * (1.0f - delay_frac) + l_b * delay_frac;
			float smp = mix_dry * in_smp + mix_wet * chorus_l;
			old_ptr = (old_ptr + 1) & CHORUS_DELAY_MASK;
			return smp;
		}
	};
	static void RunConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback);

	const struct {
		float delay_ms = 25.0;
		float depth_ms = 1.0;
		float lfo_freq = 0.8;
		float drywet = 1.0;
	}defaults_chorus;

	class dsp_chorus : public dsp_impl_base
	{
		int m_rate, m_ch, m_ch_mask;
		float delay_ms, depth_ms, lfo_freq, drywet;
		pfc::array_t<Chorus> m_buffers;
		bool enabled;


		
	public:
		dsp_chorus(dsp_preset const & in) :m_rate(0), m_ch(0), m_ch_mask(0) {
			// Mark buffer as empty.
			enabled = true;
			delay_ms = defaults_chorus.delay_ms;
			depth_ms = defaults_chorus.depth_ms;
			lfo_freq = defaults_chorus.lfo_freq;
			drywet = defaults_chorus.drywet;
			parse_preset(delay_ms, depth_ms, lfo_freq, drywet, enabled, in);
		}

		// Every DSP type is identified by a GUID.
		static GUID g_get_guid() {
			return guid_chorus;
		}

		// We also need a name, so the user can identify the DSP.
		// The name we use here does not describe what the DSP does,
		// so it would be a bad name. We can excuse this, because it
		// doesn't do anything useful anyway.
		static void g_get_name(pfc::string_base & p_out) {
			p_out = "Chorus";
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
					Chorus & e = m_buffers[i];
					e.init(delay_ms, depth_ms, lfo_freq, drywet, m_rate);
				}
			}

			for (unsigned i = 0; i < m_ch; i++)
			{
				Chorus & e = m_buffers[i];
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
			return 0.0;
		}

		virtual bool need_track_change_mark() {
			return false;
		}
		static bool g_get_default_preset(dsp_preset & p_out)
		{
			make_preset(defaults_chorus.delay_ms, defaults_chorus.depth_ms, 
				defaults_chorus.lfo_freq, defaults_chorus.drywet, true, p_out);
			return true;
		}
		static void g_show_config_popup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
		{
			::RunConfigPopup(p_data, p_parent, p_callback);
		}
		static bool g_have_config_popup() { return true; }

		static void make_preset(float delay_ms, float depth_ms, float lfo_freq, float drywet, bool enabled, dsp_preset & out)
		{
			dsp_preset_builder builder;
			builder << delay_ms;
			builder << depth_ms;
			builder << lfo_freq;
			builder << drywet;
			builder << enabled;
			builder.finish(g_get_guid(), out);
		}

		static void parse_preset(float & delay_ms, float & depth_ms, float & lfo_freq, float & drywet, bool & enabled, const dsp_preset & in)
		{
			try
			{
				dsp_preset_parser parser(in);
				parser >> delay_ms;
				parser >> depth_ms;
				parser >> lfo_freq;
				parser >> drywet;
				parser >> enabled;
			}
			catch (exception_io_data) {
				delay_ms = defaults_chorus.delay_ms; depth_ms = defaults_chorus.depth_ms; lfo_freq = defaults_chorus.lfo_freq; 
				drywet = defaults_chorus.drywet; enabled = true;
			}
		}
	};

	class CMyDSPPopupChorus : public CDialogImpl<CMyDSPPopupChorus>
	{
	public:
		CMyDSPPopupChorus(const dsp_preset & initData, dsp_preset_edit_callback & callback) : m_initData(initData), m_callback(callback) { }
		enum { IDD = IDD_CHORUS };
		enum
		{
			FreqMin = 200,
			FreqMax = 4000,
			FreqRangeTotal = FreqMax - FreqMin,
			depthmin = 0,
			depthmax = 100,
		};

		BEGIN_MSG_MAP(CMyDSPPopup)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_HANDLER_EX(IDOK, BN_CLICKED, OnButton)
			COMMAND_HANDLER_EX(IDCANCEL, BN_CLICKED, OnButton)
			COMMAND_HANDLER_EX(IDC_RESETCHR1,BN_CLICKED,OnReset1)
			COMMAND_HANDLER_EX(IDC_RESETCHR2, BN_CLICKED, OnReset2)
			COMMAND_HANDLER_EX(IDC_RESETCHR3, BN_CLICKED, OnReset3)
			COMMAND_HANDLER_EX(IDC_RESETCHR4, BN_CLICKED, OnReset4)
			COMMAND_HANDLER_EX(IDC_RESETCHR5, BN_CLICKED, OnReset5)
			MSG_WM_HSCROLL(OnHScroll)
			MESSAGE_HANDLER(WM_USER, OnEditControlChange)
		END_MSG_MAP()

	private:


		void Reset(float drywet, float lfo_freq, float depth_ms, float delay_ms)
		{
			dsp_preset_impl preset;
			slider_drywet.SetPos((double)(100 * drywet));
			slider_lfofreq.SetPos((double)(100 * lfo_freq));
			slider_depthms.SetPos((double)(100 * depth_ms));
			slider_delayms.SetPos((double)(100 * delay_ms));
			dsp_chorus::make_preset(delay_ms, depth_ms, lfo_freq, drywet, true, preset);
			m_callback.on_preset_changed(preset);
			RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet);
		}

		void OnReset1(UINT, int id, CWindow)
		{
			delay_ms = defaults_chorus.delay_ms;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);

		}

		void OnReset2(UINT, int id, CWindow)
		{
			depth_ms = defaults_chorus.depth_ms;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);
		}

		void OnReset3(UINT, int id, CWindow)
		{
			lfo_freq = defaults_chorus.lfo_freq;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);
		}

		void OnReset4(UINT, int id, CWindow)
		{
			drywet = defaults_chorus.drywet;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);
		}

		void OnReset5(UINT, int id, CWindow)
		{
			dsp_preset_impl preset;
			drywet = defaults_chorus.drywet;
			lfo_freq = defaults_chorus.lfo_freq;
			depth_ms = defaults_chorus.depth_ms;
			delay_ms = defaults_chorus.delay_ms;
			slider_drywet.SetPos((double)(100 * drywet));
			slider_lfofreq.SetPos((double)(100 * lfo_freq));
			slider_depthms.SetPos((double)(100 * depth_ms));
			slider_delayms.SetPos((double)(100 * delay_ms));
			dsp_chorus::make_preset(delay_ms, depth_ms, lfo_freq, drywet, true, preset);
			m_callback.on_preset_changed(preset);
			RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet);
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
			CString text,text2,text3,text4;
			delay_edit.GetWindowText(text);
			float delay2 = _ttof(text);
			if (delay_s != text)
			{
				preset_changed = true;
				delay_ms = delay2;
			}
				

			depth_edit.GetWindowText(text2);
			float depth2 = _ttof(text2);
			if (depth_s != text2)
			{
				preset_changed = true;
				depth_ms = depth2;
			}
				

			lfo_edit.GetWindowText(text3);
			float lfo = _ttof(text3);
			if (lfo_s != text3)
			{
				preset_changed = true;
				lfo_freq = lfo;
			}
				
			drywet_edit.GetWindowText(text4);
			float drywet2 = _ttof(text4);
			if (drywet_s != text4)
			{
				preset_changed = true;
				drywet = drywet2;
			}
				
			if(preset_changed)
			{
				dsp_chorus::make_preset(delay_ms, depth_ms, lfo_freq, drywet, true, preset);
				m_callback.on_preset_changed(preset);
				slider_drywet.SetPos((double)(100 * drywet));
				slider_lfofreq.SetPos((double)(100 * lfo_freq));
				slider_depthms.SetPos((double)(100 * depth_ms));
				slider_delayms.SetPos((double)(100 * delay_ms));
			}
		}

		BOOL OnInitDialog(CWindow, LPARAM)
		{
			
			delay_edit.AttachToDlgItem(m_hWnd);
			delay_edit.SubclassWindow(GetDlgItem(IDC_CHORUSDELAYLAB));
			depth_edit.AttachToDlgItem(m_hWnd);
			depth_edit.SubclassWindow(GetDlgItem(IDC_CHORUSDEPTHMSLAB));
			lfo_edit.AttachToDlgItem(m_hWnd);
			lfo_edit.SubclassWindow(GetDlgItem(IDC_CHORUSLFOFREQLAB));
			drywet_edit.AttachToDlgItem(m_hWnd);
			drywet_edit.SubclassWindow(GetDlgItem(IDC_CHORUSDRYWETLAB));


			slider_delayms = GetDlgItem(IDC_CHORUSDELAYMS);
			slider_delayms.SetRange(FreqMin, FreqMax);
			slider_depthms = GetDlgItem(IDC_CHORUSDEPTHMS);
			slider_depthms.SetRange(FreqMin, FreqMax);
			slider_lfofreq = GetDlgItem(IDC_CHORUSLFOFREQ);
			slider_lfofreq.SetRange(0, 5000);
			slider_drywet = GetDlgItem(IDC_CHORUSDRYWET);
			slider_drywet.SetRange(0, depthmax);
			{
				bool enabled;
				dsp_chorus::parse_preset(delay_ms, depth_ms, lfo_freq, drywet, enabled, m_initData);
				slider_delayms.SetPos((double)(100 * delay_ms));
				slider_depthms.SetPos((double)(100 * depth_ms));
				slider_lfofreq.SetPos((double)(100 * lfo_freq));
				slider_drywet.SetPos((double)(100 * drywet));
				RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet);
			}

			return TRUE;
		}

		void OnButton(UINT, int id, CWindow)
		{
			EndDialog(id);
		}

		void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar pScrollBar)
		{
			delay_ms = slider_delayms.GetPos() / 100.0;
			depth_ms = slider_depthms.GetPos() / 100.0;
			lfo_freq = slider_lfofreq.GetPos() / 100.0;
			drywet = slider_drywet.GetPos() / 100.0;
			if (LOWORD(nSBCode) != SB_THUMBTRACK)
			{
				dsp_preset_impl preset;
				dsp_chorus::make_preset(delay_ms, depth_ms, lfo_freq, drywet, true, preset);
				m_callback.on_preset_changed(preset);
			}
			RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet*100);

		}

		void RefreshLabel(float delay_ms, float depth_ms, float lfo_freq, float drywet)
		{
			CString sWindowText;
			pfc::string_formatter msg;
			msg << pfc::format_float(delay_ms, 0, 2);

			
			sWindowText = msg.c_str();
			delay_s = sWindowText;
			delay_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_float(depth_ms, 0, 2);
			sWindowText = msg.c_str();
			depth_s = sWindowText;
			depth_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_float(lfo_freq, 0, 2);
			sWindowText = msg.c_str();
			lfo_s = sWindowText;
			lfo_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_float(drywet,0,2);
			sWindowText = msg.c_str();
			drywet_s = sWindowText;
			drywet_edit.SetWindowText(sWindowText);

		}

		CEditMod delay_edit,depth_edit,lfo_edit,drywet_edit;
		CString delay_s,depth_s,lfo_s,drywet_s;

		const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
		dsp_preset_edit_callback & m_callback;
		float delay_ms, depth_ms, lfo_freq, drywet;
		CTrackBarCtrl slider_depthms, slider_delayms, slider_lfofreq, slider_drywet;
	};

	static void RunConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
	{
		CMyDSPPopupChorus popup(p_data, p_callback);
		if (popup.DoModal(p_parent) != IDOK) p_callback.on_preset_changed(p_data);
	}

	static dsp_factory_t<dsp_chorus> g_dsp_chorus_factory;

	// {1DC17CA0-0023-4266-AD59-691D566AC291}
	static const GUID guid_choruselem =
	{ 0x5bf17f6, 0x67b2, 0x42e9,{ 0xb8, 0x54, 0xa0, 0xd0, 0xf8, 0xce, 0xd4, 0xde } };


	class uielem_chorus : public CDialogImpl<uielem_chorus>, public ui_element_instance {
	public:
		uielem_chorus(ui_element_config::ptr cfg, ui_element_instance_callback::ptr cb) : m_callback(cb) {
			delay_ms = 25.0;
			depth_ms = 1.0;
			lfo_freq = 0.8;
			drywet = 1.;
			echo_enabled = true;
		}
		enum { IDD = IDD_CHORUS1 };
		enum
		{
			FreqMin = 200,
			FreqMax = 4000,
			FreqRangeTotal = FreqMax - FreqMin,
			depthmin = 0,
			depthmax = 100,
		};
	private:

		BEGIN_MSG_MAP_EX(uielem_chorus)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_HANDLER_EX(IDC_CHORUSENABLED, BN_CLICKED, OnEnabledToggle)
			COMMAND_HANDLER_EX(IDC_RESETCHRUI1, BN_CLICKED, OnReset1)
			COMMAND_HANDLER_EX(IDC_RESETCHRUI2, BN_CLICKED, OnReset2)
			COMMAND_HANDLER_EX(IDC_RESETCHRUI3, BN_CLICKED, OnReset3)
			COMMAND_HANDLER_EX(IDC_RESETCHRUI4, BN_CLICKED, OnReset4)
			COMMAND_HANDLER_EX(IDC_RESETCHRUI5, BN_CLICKED, OnReset5)
			MSG_WM_HSCROLL(OnScroll)
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
		static void g_get_name(pfc::string_base & out) { out = "Chorus"; }
		static ui_element_config::ptr g_get_default_configuration() {
			return makeConfig(true);
		}
		static const char * g_get_description() { return "Modifies the 'Chorus' DSP effect."; }
		static GUID g_get_subclass() {
			return ui_element_subclass_dsp;
		}

		ui_element_min_max_info get_min_max_info() {
			ui_element_min_max_info ret;

			// Note that we play nicely with separate horizontal & vertical DPI.
			// Such configurations have not been ever seen in circulation, but nothing stops us from supporting such.
			SIZE DPI = QueryScreenDPIEx(*this);

			if (DPI.cx <= 0 || DPI.cy <= 0) { // sanity
				DPI = CSize(96, 96);
			}


			ret.m_min_width = MulDiv(520, DPI.cx, 96);
			ret.m_min_height = MulDiv(220, DPI.cy, 96);
			ret.m_max_width = MulDiv(520, DPI.cx, 96);
			ret.m_max_height = MulDiv(220, DPI.cy, 96);

			// Deal with WS_EX_STATICEDGE and alike that we might have picked from host
			ret.adjustForWindow(*this);

			return ret;
		}

	private:
		void Reset(float drywet, float lfo_freq, float depth_ms, float delay_ms)
		{
			slider_drywet.SetPos((double)(100 * drywet));
			slider_lfofreq.SetPos((double)(100 * lfo_freq));
			slider_depthms.SetPos((double)(100 * depth_ms));
			slider_delayms.SetPos((double)(100 * delay_ms));
			ApplySettings();
			RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet * 100);
		}

		void OnReset1(UINT, int id, CWindow)
		{
			delay_ms = defaults_chorus.delay_ms;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);

		}

		void OnReset2(UINT, int id, CWindow)
		{
			depth_ms = defaults_chorus.depth_ms;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);
		}

		void OnReset3(UINT, int id, CWindow)
		{
			lfo_freq = defaults_chorus.lfo_freq;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);
		}

		void OnReset4(UINT, int id, CWindow)
		{
			drywet = defaults_chorus.drywet;
			Reset(drywet, lfo_freq, depth_ms, delay_ms);
		}

		void OnReset5(UINT, int id, CWindow)
		{
			drywet = defaults_chorus.drywet;
			lfo_freq = defaults_chorus.lfo_freq;
			depth_ms = defaults_chorus.depth_ms;
			delay_ms = defaults_chorus.delay_ms;
			slider_drywet.SetPos((double)(100 * drywet));
			slider_lfofreq.SetPos((double)(100 * lfo_freq));
			slider_depthms.SetPos((double)(100 * depth_ms));
			slider_delayms.SetPos((double)(100 * delay_ms));
			ApplySettings();
			RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet * 100);
		}


		void GetEditText()
		{
			bool preset_changed = false;
			dsp_preset_impl preset;
			CString text, text2, text3, text4;
			delay_edit.GetWindowText(text);
			float delay2 = _ttof(text);
			if (delay_s != text) 
			{
				delay_ms = delay2;
				preset_changed = true;
			}

			depth_edit.GetWindowText(text2);
			float depth2 = _ttof(text2);
			if (depth_s != text2)
			{
				preset_changed = true; 
				depth_ms = depth2;
			}


			lfo_edit.GetWindowText(text3);
			float lfo = _ttof(text3);
			if (lfo_s != text3) 
			{
				preset_changed = true;
				lfo_freq = lfo;
			}

			drywet_edit.GetWindowText(text4);
			float drywet2 = _ttof(text4);
			if (drywet_s != text4) 
			{
				preset_changed = true;
				drywet = drywet2;
			}
				
			if (preset_changed)
				ApplySettings();
			
		}

		void SetEchoEnabled(bool state) { m_buttonEchoEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
		bool IsEchoEnabled() { return m_buttonEchoEnabled == NULL || m_buttonEchoEnabled.GetCheck() == BST_CHECKED; }

		void EchoDisable() {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_chorus);
		}


		void EchoEnable(float delay_ms, float depth_ms, float lfo_freq, float drywet, bool echo_enabled) {
			dsp_preset_impl preset;
			dsp_chorus::make_preset(delay_ms, depth_ms, lfo_freq, drywet, echo_enabled, preset);
			static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
		}

		void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
			pfc::vartoggle_t<bool> ownUpdate(m_ownEchoUpdate, true);
			if (IsEchoEnabled()) {
				GetConfig();
				dsp_preset_impl preset;
				dsp_chorus::make_preset(delay_ms, depth_ms, lfo_freq, drywet, echo_enabled, preset);
				//yes change api;
				static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
			}
			else {
				static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_chorus);
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
					EchoEnable(delay_ms, depth_ms, lfo_freq, drywet, echo_enabled);
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
			if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_chorus, preset)) {
				SetEchoEnabled(true);
				dsp_chorus::parse_preset(delay_ms, depth_ms, lfo_freq, drywet, echo_enabled, preset);
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
				EchoEnable(delay_ms, depth_ms, lfo_freq, drywet, echo_enabled);
			}
			else {
				EchoDisable();
			}

		}


		void GetConfig()
		{
			delay_ms = slider_delayms.GetPos() / 100.0;
			depth_ms = slider_depthms.GetPos() / 100.0;
			lfo_freq = slider_lfofreq.GetPos() / 100.0;
			drywet = slider_drywet.GetPos() / 100.0;
			echo_enabled = IsEchoEnabled();
			RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet*100);


		}

		void SetConfig()
		{
			slider_delayms.SetPos((double)(100 * delay_ms));
			slider_depthms.SetPos((double)(100 * depth_ms));
			slider_lfofreq.SetPos((double)(100 * lfo_freq));
			slider_drywet.SetPos((double)(100 * drywet));

			RefreshLabel(delay_ms, depth_ms, lfo_freq, drywet*100);

		}

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

		BOOL OnInitDialog(CWindow, LPARAM) {
			slider_delayms = GetDlgItem(IDC_CHORUSDELAYMS1);
			slider_delayms.SetRange(FreqMin, FreqMax);
			slider_depthms = GetDlgItem(IDC_CHORUSDEPTHMS1);
			slider_depthms.SetRange(FreqMin, FreqMax);
			slider_lfofreq = GetDlgItem(IDC_CHORUSLFOFREQ1);
			slider_lfofreq.SetRange(0, 5000);
			slider_drywet = GetDlgItem(IDC_CHORUSDRYWET1);
			slider_drywet.SetRange(0, depthmax);

			delay_edit.AttachToDlgItem(m_hWnd);
			delay_edit.SubclassWindow(GetDlgItem(IDC_CHORUSDELAYLAB1));
			depth_edit.AttachToDlgItem(m_hWnd);
			depth_edit.SubclassWindow(GetDlgItem(IDC_CHORUSDEPTHMSLAB1));
			lfo_edit.AttachToDlgItem(m_hWnd);
			lfo_edit.SubclassWindow(GetDlgItem(IDC_CHORUSLFOFREQLAB1));
			drywet_edit.AttachToDlgItem(m_hWnd);
			drywet_edit.SubclassWindow(GetDlgItem(IDC_CHORUSDRYWETLAB1));


			m_buttonEchoEnabled = GetDlgItem(IDC_CHORUSENABLED);
			m_ownEchoUpdate = false;

			ApplySettings();
			return FALSE;
		}


		uint32_t shit;

		void RefreshLabel(float delay_ms, float depth_ms, float lfo_freq, float drywet)
		{
			CString sWindowText;
			pfc::string_formatter msg;
			msg << pfc::format_float(delay_ms, 0, 2);
			sWindowText = msg.c_str();
			delay_s = sWindowText;
			delay_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_float(depth_ms, 0, 2);
			sWindowText = msg.c_str();
			depth_s = sWindowText;
			depth_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_float(lfo_freq, 0, 2);
			sWindowText = msg.c_str();
			lfo_s = sWindowText;
			lfo_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_float(drywet, 0, 2);
			sWindowText = msg.c_str();
			drywet_s = sWindowText;
			drywet_edit.SetWindowText(sWindowText);
		}

		CEditMod delay_edit, depth_edit, lfo_edit, drywet_edit;
		CString delay_s, depth_s, lfo_s, drywet_s;


		bool echo_enabled;
		float delay_ms, depth_ms, lfo_freq, drywet;
		CTrackBarCtrl slider_depthms, slider_delayms, slider_lfofreq, slider_drywet;
		CButton m_buttonEchoEnabled;
		bool m_ownEchoUpdate;
	protected:
		const ui_element_instance_callback::ptr m_callback;
	};

	class myElem_t2 : public  ui_element_impl_withpopup< uielem_chorus > {
		bool get_element_group(pfc::string_base & p_out)
		{
			p_out = "Effect DSP";
			return true;
		}

		bool get_menu_command_description(pfc::string_base & out) {
			out = "Opens a window for chorus effects.";
			return true;
		}

	};
	static service_factory_single_t<myElem_t2> g_myElemFactory;

}
