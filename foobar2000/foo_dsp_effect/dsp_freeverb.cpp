#include <math.h>
#include "../helpers/foobar2000+atl.h"
#include <coreDarkMode.h>
#include "../../libPPUI/win32_utility.h"
#include "../../libPPUI/win32_op.h" // WIN32_OP()
#include "../SDK/ui_element.h"
#include "../helpers/BumpableElem.h"
#include "../../libPPUI/CDialogResizeHelper.h"
#include "resource.h"
#include "freeverb.h"
#include "dsp_guids.h"

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

	static void RunDSPConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback);
	class dsp_freeverb : public dsp_impl_base
	{
		int m_rate, m_ch, m_ch_mask;
		float drytime;
		float wettime;
		float dampness;
		float roomwidth;
		float roomsize;
		bool enabled;
		pfc::array_t<revmodel> m_buffers;
	public:

		dsp_freeverb(dsp_preset const & in) : drytime(0.43), wettime(0.57), dampness(0.45), roomwidth(0.56), roomsize(0.56), m_rate(0), m_ch(0), m_ch_mask(0)
		{
			enabled = true;
			parse_preset(drytime, wettime, dampness, roomwidth, roomsize, enabled, in);
		}
		static GUID g_get_guid()
		{
			// {97C60D5F-3572-4d35-9260-FD0CF5DBA480}

			return guid_freeverb;
		}
		static void g_get_name(pfc::string_base & p_out) { p_out = "Reverb"; }
		bool on_chunk(audio_chunk * chunk, abort_callback &)
		{
			if (!enabled)return true;
			if (chunk->get_srate() != m_rate || chunk->get_channels() != m_ch || chunk->get_channel_config() != m_ch_mask)
			{
				m_rate = chunk->get_srate();
				m_ch = chunk->get_channels();
				m_ch_mask = chunk->get_channel_config();
				m_buffers.set_count(0);
				m_buffers.set_count(m_ch);
				for (unsigned i = 0; i < m_ch; i++)
				{
					revmodel & e = m_buffers[i];
					e.init(m_rate,i==1);
					e.setwet(wettime);
					e.setdry(drytime);
					e.setdamp(dampness);
					e.setroomsize(roomsize);
					e.setwidth(roomwidth);
				}
			}

			for (unsigned i = 0; i < m_ch; i++)
			{
				revmodel& e = m_buffers[i];
				audio_sample* data = chunk->get_data() + i;
				for (unsigned j = 0, k = chunk->get_sample_count(); j < k; j++)
				{
					*data = e.processsample(*data);
					data += m_ch;
				}
			}
			return true;
			
		}
		void on_endofplayback(abort_callback &) { }
		void on_endoftrack(abort_callback &) { }
		void flush()
		{
			m_buffers.set_count(0);
			m_rate = 0;
			m_ch = 0;
			m_ch_mask = 0;
		}
		double get_latency()
		{
			return 0;
		}
		bool need_track_change_mark()
		{
			return false;
		}
		static bool g_get_default_preset(dsp_preset & p_out)
		{
			make_preset(0.43, 0.57, 0.45, 0.56, 0.56, true, p_out);
			return true;
		}
		static void g_show_config_popup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
		{
			::RunDSPConfigPopup(p_data, p_parent, p_callback);
		}
		static bool g_have_config_popup() { return true; }
		static void make_preset(float drytime, float wettime, float dampness, float roomwidth, float roomsize, bool enabled, dsp_preset & out)
		{
			dsp_preset_builder builder;
			builder << drytime;
			builder << wettime;
			builder << dampness;
			builder << roomwidth;
			builder << roomsize;
			builder << enabled;
			builder.finish(g_get_guid(), out);
		}
		static void parse_preset(float & drytime, float & wettime, float & dampness, float & roomwidth, float & roomsize, bool & enabled, const dsp_preset & in)
		{
			try
			{
				dsp_preset_parser parser(in);
				parser >> drytime;
				parser >> wettime;
				parser >> dampness;
				parser >> roomwidth;
				parser >> roomsize;
				parser >> enabled;
			}
			catch (exception_io_data) { drytime = 0.43; wettime = 0.57; dampness = 0.45; roomwidth = 0.56; roomsize = 0.56; enabled = true; }
		}
	};

	// {1DC17CA0-0023-4266-AD59-691D566AC291}
	static const GUID guid_choruselem =
	{ 0x9afc1e0, 0xe9bb, 0x487b,{ 0x9b, 0xd8, 0x11, 0x3f, 0x29, 0x48, 0x8a, 0x90 } };

	static const CDialogResizeHelper::Param chorus_uiresize[] = {
		// Dialog resize handling matrix, defines how the controls scale with the dialog
		//			 L T R B
		{IDC_STATIC1, 0,0,0,0  },
		{IDC_STATIC2,    0,0,0,0 },
		{IDC_STATIC3,    0,0,0,0 },
		{IDC_STATIC4,    0,0,0,0  },
		{IDC_STATIC5,    0,0,0,0  },
		{IDC_EDITFREEWET1, 0,0,0,0 },
		{IDC_EDITFREEDRY1,  0,0,0,0 },
	{IDC_EDITFREEDAMP1,  0,0,0,0 },
	{IDC_EDITFREEW1,  0,0,0,0 },
	{IDC_EDITFREERS1, 0,0,0,0 },
	{IDC_FREEVERBENABLE,0,0,0,0 },
	{IDC_RESETCHR5,0,0,0,0 },
	{IDC_DRYTIME1, 0,0,1,0},
	{IDC_WETTIME1, 0,0,1,0},
	{IDC_ROOMWIDTH1, 0,0,1,0},
	{IDC_DAMPING1, 0,0,1,0},
	{IDC_ROOMSIZE1,0,0,1,0},
	// current position of a control is determined by initial_position + factor * (current_dialog_size - initial_dialog_size)
	// where factor is the value from the table above
	// applied to all four values - left, top, right, bottom
	// 0,0,0,0 means that a control doesn't react to dialog resizing (aligned to top+left, no resize)
	// 1,1,1,1 means that the control is aligned to bottom+right but doesn't resize
	// 0,0,1,0 means that the control disregards vertical resize (aligned to top) and changes its width with the dialog
	};
	static const CRect resizeMinMax(200, 200, 1000, 1000);

	class uielem_freeverb : public CDialogImpl<uielem_freeverb>, public ui_element_instance {
	public:
		uielem_freeverb(ui_element_config::ptr cfg, ui_element_instance_callback::ptr cb) : m_callback(cb), m_resizer(chorus_uiresize, resizeMinMax) {
			drytime = 0.43; wettime = 0.57; dampness = 0.45;
			roomwidth = 0.56; roomsize = 0.56; reverb_enabled = true;

		}
		enum { IDD = IDD_REVERB1 };
		enum
		{
			drytimemin = 0,
			drytimemax = 100,
			drytimetotal = 100,
			wettimemin = 0,
			wettimemax = 100,
			wettimetotal = 100,
			dampnessmin = 0,
			dampnessmax = 100,
			dampnesstotal = 100,
			roomwidthmin = 0,
			roomwidthmax = 100,
			roomwidthtotal = 100,
			roomsizemin = 0,
			roomsizemax = 100,
			roomsizetotal = 100
		};

		BEGIN_MSG_MAP(uielem_freeverb)
			CHAIN_MSG_MAP_MEMBER(m_resizer)
			MSG_WM_INITDIALOG(OnInitDialog)
			COMMAND_HANDLER_EX(IDC_FREEVERBENABLE, BN_CLICKED, OnEnabledToggle)
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
		static void g_get_name(pfc::string_base & out) { out = "Reverb"; }
		static ui_element_config::ptr g_get_default_configuration() {
			return makeConfig(true);
		}
		static const char * g_get_description() { return "Modifies the reverberation DSP effect."; }
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


			ret.m_min_width = MulDiv(200, DPI.cx, 96);
			ret.m_min_height = MulDiv(200, DPI.cy, 96);
			ret.m_max_width = MulDiv(1000, DPI.cx, 96);
			ret.m_max_height = MulDiv(1000, DPI.cy, 96);

			// Deal with WS_EX_STATICEDGE and alike that we might have picked from host
			ret.adjustForWindow(*this);

			return ret;
		}

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
			drytime = 0.43; wettime = 0.57; dampness = 0.45;
			roomwidth = 0.56; roomsize = 0.56; reverb_enabled = true;
			SetConfig();
			if(IsReverbEnabled())
			OnConfigChanged();
		}

		void GetEditText()
		{
			bool preset_changed = false;
			CString text, text2, text3, text4, text5;

			drytime_edit.GetWindowText(text);
			wettime_edit.GetWindowText(text2);
			dampness_edit.GetWindowText(text3);
			roomwidth_edit.GetWindowText(text4);
			roomsize_edit.GetWindowText(text5);

			float drytime2 = pfc::clip_t<t_int32>(_ttoi(text), 0, 100)/100.0;

			if (drytime_s != text)
			{
				drytime = drytime2;
				preset_changed = true;
			}

			float wettime2 = pfc::clip_t<t_int32>(_ttoi(text2), 0, 100) / 100.0;

			if (wettime_s != text)
			{
				wettime = wettime2;
				preset_changed = true;
			}

			float dampness2 = pfc::clip_t<t_int32>(_ttoi(text3), 0, 100) / 100.0;

			if (dampness_s != text)
			{
				dampness = dampness2;
				preset_changed = true;
			}

			float roomwidth2 = pfc::clip_t<t_int32>(_ttoi(text4), 0, 100) / 100.0;

			if (roomwidth_s != text)
			{
				roomwidth = roomwidth2;
				preset_changed = true;
			}

			float roomsize2 = pfc::clip_t<t_int32>(_ttoi(text5), 0, 100) / 100.0;

			if (roomsize_s != text)
			{
				roomsize = roomsize2;
				preset_changed = true;
			}

			if(preset_changed)
			{
				SetConfig();
				OnConfigChanged();
			}
		}


		fb2k::CCoreDarkModeHooks m_hooks;
		void SetReverbEnabled(bool state) { m_buttonReverbEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
		bool IsReverbEnabled() { return m_buttonReverbEnabled == NULL || m_buttonReverbEnabled.GetCheck() == BST_CHECKED; }

		void ReverbDisable() {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_freeverb);
		}


		void ReverbEnable(float  drytime, float wettime, float dampness, float roomwidth, float roomsize, bool reverb_enabled) {
			dsp_preset_impl preset;
			dsp_freeverb::make_preset(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled, preset);
			static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
		}

		void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
			pfc::vartoggle_t<bool> ownUpdate(m_ownReverbUpdate, true);
			if (IsReverbEnabled()) {
				GetConfig();
				dsp_preset_impl preset;
				dsp_freeverb::make_preset(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled, preset);
				//yes change api;
				static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
			}
			else {
				static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_freeverb);
			}

		}

		void OnScroll(UINT scrollID, int pos, CWindow window)
		{
			pfc::vartoggle_t<bool> ownUpdate(m_ownReverbUpdate, true);
			GetConfig();
			if (IsReverbEnabled())
			{
				if (LOWORD(scrollID) != SB_THUMBTRACK)
				{
					ReverbEnable(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled);
				}
			}

		}

		void OnChange(UINT, int id, CWindow)
		{
			pfc::vartoggle_t<bool> ownUpdate(m_ownReverbUpdate, true);
			GetConfig();
			if (IsReverbEnabled())
			{

				OnConfigChanged();
			}
		}

		void DSPConfigChange(dsp_chain_config const & cfg)
		{
			if (!m_ownReverbUpdate && m_hWnd != NULL) {
				ApplySettings();
			}
		}

		//set settings if from another control
		void ApplySettings()
		{
			dsp_preset_impl preset;
			if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_freeverb, preset)) {
				SetReverbEnabled(true);
				dsp_freeverb::parse_preset(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled, preset);
				SetReverbEnabled(reverb_enabled);
				SetConfig();
			}
			else {
				SetReverbEnabled(false);
				SetConfig();
			}
		}

		void OnConfigChanged() {
			if (IsReverbEnabled()) {
				ReverbEnable(drytime, wettime, dampness, roomwidth, roomsize, reverb_enabled);
			}
			else {
				ReverbDisable();
			}

		}


		void GetConfig()
		{
			drytime = slider_drytime.GetPos() / 100.0;
			wettime = slider_wettime.GetPos() / 100.0;
			dampness = slider_dampness.GetPos() / 100.0;
			roomwidth = slider_roomwidth.GetPos() / 100.0;
			roomsize = slider_roomsize.GetPos() / 100.0;
			reverb_enabled = IsReverbEnabled();
			RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);
		}

		void SetConfig()
		{
			slider_drytime.SetPos((double)(100 * drytime));
			slider_wettime.SetPos((double)(100 * wettime));
			slider_dampness.SetPos((double)(100 * dampness));
			slider_roomwidth.SetPos((double)(100 * roomwidth));
			slider_roomsize.SetPos((double)(100 * roomsize));
			RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);

		}

		BOOL OnInitDialog(CWindow, LPARAM)
		{
			slider_drytime = GetDlgItem(IDC_DRYTIME1);
			slider_drytime.SetRange(0, drytimetotal);
			slider_wettime = GetDlgItem(IDC_WETTIME1);
			slider_wettime.SetRange(0, wettimetotal);
			slider_dampness = GetDlgItem(IDC_DAMPING1);
			slider_dampness.SetRange(0, dampnesstotal);
			slider_roomwidth = GetDlgItem(IDC_ROOMWIDTH1);
			slider_roomwidth.SetRange(0, roomwidthtotal);
			slider_roomsize = GetDlgItem(IDC_ROOMSIZE1);
			slider_roomsize.SetRange(0, roomsizetotal);

			drytime_edit.AttachToDlgItem(m_hWnd);
			drytime_edit.SubclassWindow(GetDlgItem(IDC_EDITFREEDRY1));
			wettime_edit.AttachToDlgItem(m_hWnd);
			wettime_edit.SubclassWindow(GetDlgItem(IDC_EDITFREEWET1));
			dampness_edit.AttachToDlgItem(m_hWnd);
			dampness_edit.SubclassWindow(GetDlgItem(IDC_EDITFREEDAMP1));
			roomwidth_edit.AttachToDlgItem(m_hWnd);
			roomwidth_edit.SubclassWindow(GetDlgItem(IDC_EDITFREEW1));
			roomsize_edit.AttachToDlgItem(m_hWnd);
			roomsize_edit.SubclassWindow(GetDlgItem(IDC_EDITFREERS1));

			m_buttonReverbEnabled = GetDlgItem(IDC_FREEVERBENABLE);
			m_ownReverbUpdate = false;

			ApplySettings();
			m_hooks.AddDialogWithControls(m_hWnd);
			return TRUE;
		}

		void RefreshLabel(float  drytime, float wettime, float dampness, float roomwidth, float roomsize)
		{
			CString sWindowText;
			pfc::string_formatter msg;
			msg << pfc::format_int(drytime*100);
			sWindowText = msg.c_str();
			drytime_s = sWindowText;
			drytime_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(wettime * 100);
			sWindowText = msg.c_str();
			wettime_s = sWindowText;
			wettime_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(dampness * 100);
			sWindowText = msg.c_str();
			dampness_s = sWindowText;
			dampness_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(roomwidth * 100);
			sWindowText = msg.c_str();
			roomwidth_s = sWindowText;
			roomwidth_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(roomsize * 100);
			sWindowText = msg.c_str();
			roomsize_s = sWindowText;
			roomsize_edit.SetWindowText(sWindowText);
		}

		bool reverb_enabled;
		float  drytime, wettime, dampness, roomwidth, roomsize;
		CDialogResizeHelper m_resizer;
		CEditMod drytime_edit, wettime_edit, dampness_edit, roomwidth_edit, roomsize_edit;
		CString  drytime_s, wettime_s, dampness_s, roomwidth_s, roomsize_s;
		CTrackBarCtrl slider_drytime, slider_wettime, slider_dampness, slider_roomwidth, slider_roomsize;
		CButton m_buttonReverbEnabled;
		bool m_ownReverbUpdate;

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

	class myElem_t : public  ui_element_impl_withpopup< uielem_freeverb > {
		bool get_element_group(pfc::string_base & p_out)
		{
			p_out = "Effect DSP";
			return true;
		}

		bool get_menu_command_description(pfc::string_base & out) {
			out = "Opens a window for reverberation effects control.";
			return true;
		}

		bool get_popup_specs(ui_size& defSize, pfc::string_base& title)
		{
			defSize = { 200,200 };
			title = "Freeverb DSP";
			return true;
		}

	};
	static service_factory_single_t<myElem_t> g_myElemFactory;

	class CMyDSPPopupReverb : public CDialogImpl<CMyDSPPopupReverb>
	{
	public:
		CMyDSPPopupReverb(const dsp_preset & initData, dsp_preset_edit_callback & callback) : m_initData(initData), m_callback(callback) { }
		enum { IDD = IDD_REVERB };
		enum
		{
			drytimemin = 0,
			drytimemax = 100,
			drytimetotal = 100,
			wettimemin = 0,
			wettimemax = 100,
			wettimetotal = 100,
			dampnessmin = 0,
			dampnessmax = 100,
			dampnesstotal = 100,
			roomwidthmin = 0,
			roomwidthmax = 100,
			roomwidthtotal = 100,
			roomsizemin = 0,
			roomsizemax = 100,
			roomsizetotal = 100
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

		void OnReset5(UINT, int id, CWindow)
		{
			drytime = 0.43; wettime = 0.57; dampness = 0.45;
			roomwidth = 0.56; roomsize = 0.56;
			slider_drytime.SetPos((double)(100 * drytime));
			slider_wettime.SetPos((double)(100 * wettime));
			slider_dampness.SetPos((double)(100 * dampness));
			slider_roomwidth.SetPos((double)(100 * roomwidth));
			slider_roomsize.SetPos((double)(100 * roomsize));
			dsp_preset_impl preset;
			dsp_freeverb::make_preset(drytime, wettime, dampness, roomwidth, roomsize, true, preset);
			m_callback.on_preset_changed(preset);
			RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);
		}

		LRESULT OnEditControlChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
		{
			if (wParam == 0x1988)
			{
				GetEditText();
			}
			return 0;
		}


		void DSPConfigChange(dsp_chain_config const & cfg)
		{
			if (m_hWnd != NULL) {
				ApplySettings();
			}
		}

		void GetEditText()
		{
			bool preset_changed = false;
			CString text, text2, text3, text4, text5;

			drytime_edit.GetWindowText(text);
			wettime_edit.GetWindowText(text2);
			dampness_edit.GetWindowText(text3);
			roomwidth_edit.GetWindowText(text4);
			roomsize_edit.GetWindowText(text5);

			float drytime2 = pfc::clip_t<t_int32>(_ttoi(text), 0, 100) / 100.0;

			if (drytime_s != text)
			{
				drytime = drytime2;
				preset_changed = true;
			}

			float wettime2 = pfc::clip_t<t_int32>(_ttoi(text2), 0, 100) / 100.0;

			if (wettime_s != text)
			{
				wettime = wettime2;
				preset_changed = true;
			}

			float dampness2 = pfc::clip_t<t_int32>(_ttoi(text3), 0, 100) / 100.0;

			if (dampness_s != text)
			{
				dampness = dampness2;
				preset_changed = true;
			}

			float roomwidth2 = pfc::clip_t<t_int32>(_ttoi(text4), 0, 100) / 100.0;

			if (roomwidth_s != text)
			{
				roomwidth = roomwidth2;
				preset_changed = true;
			}

			float roomsize2 = pfc::clip_t<t_int32>(_ttoi(text5), 0, 100) / 100.0;

			if (roomsize_s != text)
			{
				roomsize = roomsize2;
				preset_changed = true;
			}

			if (preset_changed)
			{
				slider_drytime.SetPos((double)(100 * drytime));
				slider_wettime.SetPos((double)(100 * wettime));
				slider_dampness.SetPos((double)(100 * dampness));
				slider_roomwidth.SetPos((double)(100 * roomwidth));
				slider_roomsize.SetPos((double)(100 * roomsize));
				dsp_preset_impl preset;
				dsp_freeverb::make_preset(drytime, wettime, dampness, roomwidth, roomsize, true, preset);
				m_callback.on_preset_changed(preset);
			    RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);
			}
		}

		void ApplySettings()
		{
			dsp_preset_impl preset2;
			if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_freeverb, preset2)) {
				bool enabled;
				dsp_freeverb::parse_preset(drytime, wettime, dampness, roomwidth, roomsize, enabled, m_initData);

				slider_drytime.SetPos((double)(100 * drytime));
				slider_wettime.SetPos((double)(100 * wettime));
				slider_dampness.SetPos((double)(100 * dampness));
				slider_roomwidth.SetPos((double)(100 * roomwidth));
				slider_roomsize.SetPos((double)(100 * roomsize));

				RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);
			}
		}

		fb2k::CCoreDarkModeHooks m_hooks;

		BOOL OnInitDialog(CWindow, LPARAM)
		{
			slider_drytime = GetDlgItem(IDC_DRYTIME);
			slider_drytime.SetRange(0, drytimetotal);
			slider_wettime = GetDlgItem(IDC_WETTIME);
			slider_wettime.SetRange(0, wettimetotal);
			slider_dampness = GetDlgItem(IDC_DAMPING);
			slider_dampness.SetRange(0, dampnesstotal);
			slider_roomwidth = GetDlgItem(IDC_ROOMWIDTH);
			slider_roomwidth.SetRange(0, roomwidthtotal);
			slider_roomsize = GetDlgItem(IDC_ROOMSIZE);
			slider_roomsize.SetRange(0, roomsizetotal);

			drytime_edit.AttachToDlgItem(m_hWnd);
			drytime_edit.SubclassWindow(GetDlgItem(IDC_EDITFREEDRY));
			wettime_edit.AttachToDlgItem(m_hWnd);
			wettime_edit.SubclassWindow(GetDlgItem(IDC_EDITFREEWET));
			dampness_edit.AttachToDlgItem(m_hWnd);
			dampness_edit.SubclassWindow(GetDlgItem(IDC_EDITFREEDAMP));
			roomwidth_edit.AttachToDlgItem(m_hWnd);
			roomwidth_edit.SubclassWindow(GetDlgItem(IDC_EDITFREERW));
			roomsize_edit.AttachToDlgItem(m_hWnd);
			roomsize_edit.SubclassWindow(GetDlgItem(IDC_EDITFREERSZ));

			{
				bool enabled;
				dsp_freeverb::parse_preset(drytime, wettime, dampness, roomwidth, roomsize, enabled, m_initData);

				slider_drytime.SetPos((double)(100 * drytime));
				slider_wettime.SetPos((double)(100 * wettime));
				slider_dampness.SetPos((double)(100 * dampness));
				slider_roomwidth.SetPos((double)(100 * roomwidth));
				slider_roomsize.SetPos((double)(100 * roomsize));

				RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);

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
			drytime = slider_drytime.GetPos() / 100.0;
			wettime = slider_wettime.GetPos() / 100.0;
			dampness = slider_dampness.GetPos() / 100.0;
			roomwidth = slider_roomwidth.GetPos() / 100.0;
			roomsize = slider_roomsize.GetPos() / 100.0;
			if (LOWORD(nSBCode) != SB_THUMBTRACK)
			{
				dsp_preset_impl preset;
				dsp_freeverb::make_preset(drytime, wettime, dampness, roomwidth, roomsize, true, preset);
				m_callback.on_preset_changed(preset);
			}
			RefreshLabel(drytime, wettime, dampness, roomwidth, roomsize);
		}

		void RefreshLabel(float  drytime, float wettime, float dampness, float roomwidth, float roomsize)
		{
			CString sWindowText;
			pfc::string_formatter msg;
			msg << pfc::format_int(drytime * 100);
			sWindowText = msg.c_str();
			drytime_s = sWindowText;
			drytime_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(wettime * 100);
			sWindowText = msg.c_str();
			wettime_s = sWindowText;
			wettime_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(dampness * 100);
			sWindowText = msg.c_str();
			dampness_s = sWindowText;
			dampness_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(roomwidth * 100);
			sWindowText = msg.c_str();
			roomwidth_s = sWindowText;
			roomwidth_edit.SetWindowText(sWindowText);
			msg.reset();
			msg << pfc::format_int(roomsize * 100);
			sWindowText = msg.c_str();
			roomsize_s = sWindowText;
			roomsize_edit.SetWindowText(sWindowText);
		}
		const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
		dsp_preset_edit_callback & m_callback;
		float  drytime, wettime, dampness, roomwidth, roomsize;



		CEditMod drytime_edit, wettime_edit, dampness_edit, roomwidth_edit, roomsize_edit;
		CString  drytime_s, wettime_s, dampness_s, roomwidth_s, roomsize_s;
		CTrackBarCtrl slider_drytime, slider_wettime, slider_dampness, slider_roomwidth, slider_roomsize;
	};
	static void RunDSPConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
	{
		CMyDSPPopupReverb popup(p_data, p_callback);
		if (popup.DoModal(p_parent) != IDOK) p_callback.on_preset_changed(p_data);
	}

	static dsp_factory_t<dsp_freeverb> g_dsp_reverb_factory;

}
