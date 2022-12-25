#include "../helpers/foobar2000+atl.h"
#include <coreDarkMode.h>
#include "../SDK/ui_element.h"
#include "../helpers/BumpableElem.h"
#include "../../libPPUI/CDialogResizeHelper.h"
#include "resource.h"
#include "Phaser.h"
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


static void RunConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback);
class dsp_phaser : public dsp_impl_base
{
	int m_rate, m_ch, m_ch_mask;
	float freq; //0.1 - 4.0
	float startphase;  //0 - 360
	float fb; //-100 - 100
	int depth;//0-255
	int stages; //2-24
	int drywet; //0-255
	bool enabled;
	pfc::array_t<Phaser> m_buffers;
public:
	static GUID g_get_guid()
	{
		// {8B54D803-EFEA-4b6c-B3BE-921F6ADC7221}

		return guid_phaser;
	}

	dsp_phaser(dsp_preset const & in) : m_rate(0), m_ch(0), m_ch_mask(0), freq(0.4), startphase(0), fb(0), depth(100), stages(2), drywet(128)
	{
		enabled = true;
		parse_preset(freq, startphase, fb, depth, stages, drywet, enabled, in);
	}

	static void g_get_name(pfc::string_base & p_out) { p_out = "Phaser"; }

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
				Phaser & e = m_buffers[i];
				e.SetDepth(depth);
				e.SetDryWet(drywet);
				e.SetFeedback(fb);
				e.SetLFOFreq(freq);
				e.SetLFOStartPhase(startphase);
				e.SetStages(stages);
				e.init(m_rate);
			}
		}

		for (unsigned i = 0; i < m_ch; i++)
		{
			Phaser & e = m_buffers[i];
			audio_sample * data = chunk->get_data() + i;
			for (unsigned j = 0, k = chunk->get_sample_count(); j < k; j++)
			{
				*data = e.Process(*data);
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
		make_preset(0.4, 0, 0, 100, 2, 128, true, p_out);
		return true;
	}
	static void g_show_config_popup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
	{
		::RunConfigPopup(p_data, p_parent, p_callback);
	}
	static bool g_have_config_popup() { return true; }
	static void make_preset(float freq, float startphase, float fb, int depth, int stages, int drywet, bool enabled, dsp_preset & out)
	{
		dsp_preset_builder builder;
		builder << freq;
		builder << startphase;
		builder << fb;
		builder << depth;
		builder << stages;
		builder << drywet;
		builder << enabled;
		builder.finish(g_get_guid(), out);
	}
	static void parse_preset(float & freq, float & startphase, float & fb, int & depth, int & stages, int  & drywet, bool & enabled, const dsp_preset & in)
	{
		try
		{
			dsp_preset_parser parser(in);
			parser >> freq;
			parser >> startphase;
			parser >> fb;
			parser >> depth;
			parser >> stages;
			parser >> drywet;
			parser >> enabled;
		}
		catch (exception_io_data) { freq = 0.4; startphase = 0; fb = 0; depth = 100; stages = 2; drywet = 128; enabled = true; }
	}
};


// {1DC17CA0-0023-4266-AD59-691D566AC291}
static const GUID guid_choruselem =
{ 0xec43098, 0x824d, 0x4d5c,{ 0x80, 0x72, 0x74, 0x82, 0xf8, 0xea, 0x75, 0xe4 } };

static const CDialogResizeHelper::Param chorus_uiresize[] = {
	// Dialog resize handling matrix, defines how the controls scale with the dialog
	//			 L T R B
	{IDC_STATIC1, 0,0,0,0  },
	{IDC_STATIC2,    0,0,0,0 },
	{IDC_STATIC3,    0,0,0,0 },
	{IDC_STATIC4,    0,0,0,0  },
	{IDC_STATIC5,    0,0,0,0  },
	{IDC_STATIC6,    0,0,0,0  },
	{IDC_PHASERENABLED,    0,0,0,0  },
	{IDC_EDITPHASELFOFREQUI, 0,0,0,0 },
	{IDC_EDITPHASESTARTPHASEUI,  0,0,0,0 },
{IDC_EDITPHASEFBUI,  0,0,0,0 },
{IDC_EDITPHASERSTAGESUI,  0,0,0,0 },
{IDC_EDITPHASEDRYWETUI, 0,0,0,0 },
{IDC_EDITPHASEDEPTHUI,0,0,0,0 },
{IDC_RESETCHR5,0,0,0,0 },
{IDC_PHASERSLFOFREQ1, 0,0,1,0},
{IDC_PHASERSLFOSTARTPHASE1, 0,0,1,0},
{IDC_PHASERSFEEDBACK1, 0,0,1,0},
{IDC_PHASERSDEPTH1, 0,0,1,0},
{IDC_PHASERSTAGES1,0,0,1,0},
{IDC_PHASERSDRYWET1,0,0,1,0},
// current position of a control is determined by initial_position + factor * (current_dialog_size - initial_dialog_size)
// where factor is the value from the table above
// applied to all four values - left, top, right, bottom
// 0,0,0,0 means that a control doesn't react to dialog resizing (aligned to top+left, no resize)
// 1,1,1,1 means that the control is aligned to bottom+right but doesn't resize
// 0,0,1,0 means that the control disregards vertical resize (aligned to top) and changes its width with the dialog
};
static const CRect resizeMinMax(200, 240, 1000, 1000);


class uielem_phaser : public CDialogImpl<uielem_phaser>, public ui_element_instance, private play_callback_impl_base {
public:
	uielem_phaser(ui_element_config::ptr cfg, ui_element_instance_callback::ptr cb) : m_callback(cb),m_resizer(chorus_uiresize, resizeMinMax) {
		freq = 0.4; startphase = 0;
		fb = 0; depth = 100;
		stages = 2; drywet = 128;
		phaser_enabled = true;

	}
	enum { IDD = IDD_PHASER1 };
	enum
	{
		FreqMin = 1,
		FreqMax = 200,
		FreqRangeTotal = FreqMax - FreqMin,
		StartPhaseMin = 0,
		StartPhaseMax = 360,
		StartPhaseTotal = 360,
		FeedbackMin = -100,
		FeedbackMax = 100,
		FeedbackRangeTotal = 200,
		DepthMin = 0,
		DepthMax = 256,
		DepthRangeTotal = 255,
		StagesMin = 1,
		StagesMax = 24,
		StagesRangeTotal = 24,
		DryWetMin = 0,
		DryWetMax = 256,
		DryWetRangeTotal = 255
	};
	BEGIN_MSG_MAP(uielem_phaser)
		CHAIN_MSG_MAP_MEMBER(m_resizer)
		MSG_WM_INITDIALOG(OnInitDialog)
		COMMAND_HANDLER_EX(IDC_PHASERENABLED, BN_CLICKED, OnEnabledToggle)
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
	static void g_get_name(pfc::string_base & out) { out = "Phaser"; }
	static ui_element_config::ptr g_get_default_configuration() {
		return makeConfig(true);
	}
	static const char * g_get_description() { return "Modifies the 'Phaser' DSP effect."; }
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
		ret.m_min_height = MulDiv(240, DPI.cy, 96);
		ret.m_max_width = MulDiv(1000, DPI.cx, 96);
		ret.m_max_height = MulDiv(1000, DPI.cy, 96);

		// Deal with WS_EX_STATICEDGE and alike that we might have picked from host
		ret.adjustForWindow(*this);

		return ret;
	}

private:
	void on_playback_new_track(metadb_handle_ptr p_track) { ApplySettings(); }

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
		freq = 0.4; startphase = 0;
		fb = 0; depth = 100;
		stages = 2; drywet = 128;
		phaser_enabled = true;
		slider_freq.SetPos((double)(10 * freq));
		slider_startphase.SetPos(startphase);
		slider_fb.SetPos(fb);
		slider_depth.SetPos(depth);
		slider_stages.SetPos(stages);
		slider_drywet.SetPos(drywet);
		RefreshLabel(freq, startphase, fb, depth, stages, drywet);
		OnConfigChanged();
	}


	fb2k::CCoreDarkModeHooks m_hooks;
	void SetPhaserEnabled(bool state) { m_buttonPhaserEnabled.SetCheck(state ? BST_CHECKED : BST_UNCHECKED); }
	bool IsPhaserEnabled() { return m_buttonPhaserEnabled == NULL || m_buttonPhaserEnabled.GetCheck() == BST_CHECKED; }

	void DynamicsDisable() {
		static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_phaser);
	}


	void PhaserEnable(float freq, float startphase, float fb, int depth, int stages, int drywet, bool enabled) {
		dsp_preset_impl preset;
		dsp_phaser::make_preset(freq, startphase, fb, depth, stages, drywet, enabled, preset);
		static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
	}

	void OnEnabledToggle(UINT uNotifyCode, int nID, CWindow wndCtl) {
		pfc::vartoggle_t<bool> ownUpdate(m_ownPhaserUpdate, true);
		if (IsPhaserEnabled()) {
			GetConfig();
			dsp_preset_impl preset;
			dsp_phaser::make_preset(freq, startphase, fb, depth, stages, drywet, phaser_enabled, preset);
			//yes change api;
			static_api_ptr_t<dsp_config_manager>()->core_enable_dsp(preset, dsp_config_manager::default_insert_last);
		}
		else {
			static_api_ptr_t<dsp_config_manager>()->core_disable_dsp(guid_phaser);
		}

	}

	void OnScroll(UINT scrollID, int pos, CWindow window)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownPhaserUpdate, true);
		GetConfig();
		if (IsPhaserEnabled())
		{
			if (LOWORD(scrollID) != SB_THUMBTRACK)
			{
				PhaserEnable(freq, startphase, fb, depth, stages, drywet, phaser_enabled);
			}
		}

	}

	void OnChange(UINT, int id, CWindow)
	{
		pfc::vartoggle_t<bool> ownUpdate(m_ownPhaserUpdate, true);
		GetConfig();
		if (IsPhaserEnabled())
		{

			OnConfigChanged();
		}
	}

	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (!m_ownPhaserUpdate && m_hWnd != NULL) {
			ApplySettings();
		}
	}

	//set settings if from another control
	void ApplySettings()
	{
		dsp_preset_impl preset;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_phaser, preset)) {
			SetPhaserEnabled(true);
			dsp_phaser::parse_preset(freq, startphase, fb, depth, stages, drywet, phaser_enabled, preset);
			SetPhaserEnabled(phaser_enabled);
			SetConfig();
		}
		else {
			SetPhaserEnabled(false);
			SetConfig();
		}
	}

	void OnConfigChanged() {
		if (IsPhaserEnabled()) {
			PhaserEnable(freq, startphase, fb, depth, stages, drywet, phaser_enabled);
		}
		else {
			DynamicsDisable();
		}

	}

	void GetEditText()
	{
		bool preset_changed = false;
		CString text, text2, text3, text4, text5, text6;

		freq_edit.GetWindowText(text);
		startphase_edit.GetWindowText(text2);
		fb_edit.GetWindowText(text3);
		depth_edit.GetWindowText(text4);
		stages_edit.GetWindowText(text5);
		drywet_edit.GetWindowText(text6);

		float freq2 = pfc::clip_t<t_float32>(_ttoi(text), FreqMin, FreqMax) / 10.0;

		if (freq_s != text)
		{
			freq = freq2;
			preset_changed = true;
		}

		int startphase2 = pfc::clip_t<t_int32>(_ttoi(text2), 0, StartPhaseMax);

		if (startphase_s != text2)
		{
			startphase = startphase2;
			preset_changed = true;
		}

		float fb2 = pfc::clip_t<t_int32>(_ttoi(text3), -100, 100);

		if (fb_s != text3)
		{
			fb = fb2;
			preset_changed = true;
		}

		float depth2 = pfc::clip_t<t_float32>(_ttoi(text4), 0, 100);

		if (depth_s != text4)
		{
			depth = (depth2 / 100 * 256);
			preset_changed = true;
		}

		int stages2 = pfc::clip_t<t_int32>(_ttoi(text5), StagesMin, StagesMax);

		if (stages_s != text5)
		{
			stages = stages2;
			preset_changed = true;
		}

		float drywet2 = pfc::clip_t<t_int32>(_ttoi(text6), 0, 100);

		if (drywet_s != text6)
		{
			drywet = (drywet2 / 100 * 256);
			preset_changed = true;
		}

		if (preset_changed)
		{
			slider_freq.SetPos((double)(10 * freq));
			slider_startphase.SetPos(startphase);
			slider_fb.SetPos(fb);
			slider_depth.SetPos(depth);
			slider_stages.SetPos(stages);
			slider_drywet.SetPos(drywet);
			RefreshLabel(freq, startphase, fb, depth, stages, drywet);
			OnConfigChanged();
		}
	}


	void GetConfig()
	{
		freq = slider_freq.GetPos() / 10.0;
		startphase = slider_startphase.GetPos();
		fb = slider_fb.GetPos();
		depth = slider_depth.GetPos();
		stages = slider_stages.GetPos();
		drywet = slider_drywet.GetPos();
		phaser_enabled = IsPhaserEnabled();
		RefreshLabel(freq, startphase, fb, depth, stages, drywet);


	}

	void SetConfig()
	{
		slider_freq.SetPos((double)(10 * freq));
		slider_startphase.SetPos(startphase);
		slider_fb.SetPos(fb);
		slider_depth.SetPos(depth);
		slider_stages.SetPos(stages);
		slider_drywet.SetPos(drywet);

		RefreshLabel(freq, startphase, fb, depth, stages, drywet);

	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_freq = GetDlgItem(IDC_PHASERSLFOFREQ1);
		slider_freq.SetRange(FreqMin, FreqMax);
		slider_startphase = GetDlgItem(IDC_PHASERSLFOSTARTPHASE1);
		slider_startphase.SetRange(0, StartPhaseMax);
		slider_fb = GetDlgItem(IDC_PHASERSFEEDBACK1);
		slider_fb.SetRange(-100, 100);
		slider_depth = GetDlgItem(IDC_PHASERSDEPTH1);
		slider_depth.SetRange(0, 255);
		slider_stages = GetDlgItem(IDC_PHASERSTAGES1);
		slider_stages.SetRange(StagesMin, StagesMax);
		slider_drywet = GetDlgItem(IDC_PHASERSDRYWET1);
		slider_drywet.SetRange(0, 255);

		freq_edit.AttachToDlgItem(m_hWnd);
		freq_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASELFOFREQUI));
		startphase_edit.AttachToDlgItem(m_hWnd);
		startphase_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASESTARTPHASEUI));
		fb_edit.AttachToDlgItem(m_hWnd);
		fb_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASEFBUI));
		depth_edit.AttachToDlgItem(m_hWnd);
		depth_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASEDEPTHUI));
		stages_edit.AttachToDlgItem(m_hWnd);
		stages_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASERSTAGESUI));
		drywet_edit.AttachToDlgItem(m_hWnd);
		drywet_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASEDRYWETUI));

		m_buttonPhaserEnabled = GetDlgItem(IDC_PHASERENABLED);
		m_ownPhaserUpdate = false;

		ApplySettings();
		m_hooks.AddDialogWithControls(m_hWnd);
		return TRUE;
	}


	void RefreshLabel(float freq, float startphase, float fb, int depth, int stages, int drywet)
	{
		pfc::string_formatter msg;

		msg << pfc::format_float(freq, 0, 1);
		freq_s = msg.c_str();
		freq_edit.SetWindowText(freq_s);
		msg.reset();


		msg << pfc::format_int(startphase);
		startphase_s = msg.c_str();
		startphase_edit.SetWindowText(startphase_s);
		msg.reset();

		msg << pfc::format_int(fb);
		fb_s = msg.c_str();
		fb_edit.SetWindowText(fb_s);
		msg.reset();
		msg << pfc::format_int(depth * 100 / 256);
		depth_s = msg.c_str();
		depth_edit.SetWindowText(depth_s);
		msg.reset();

		msg << pfc::format_int(stages);
		stages_s = msg.c_str();
		stages_edit.SetWindowText(stages_s);
		msg.reset();

		msg << pfc::format_int(drywet * 100 / 256);
		drywet_s = msg.c_str();
		drywet_edit.SetWindowText(drywet_s);
		msg.reset();
	}

	bool phaser_enabled;
	float freq; //0.1 - 4.0
	float startphase;  //0 - 360
	float fb; //-100 - 100
	int depth;//0-255
	int stages; //2-24
	int drywet; //0-255


	CEditMod freq_edit, startphase_edit, fb_edit, depth_edit, stages_edit, drywet_edit;
	CString freq_s, startphase_s, fb_s,depth_s,stages_s,drywet_s;
	CDialogResizeHelper m_resizer;
	CTrackBarCtrl slider_freq, slider_startphase, slider_fb, slider_depth, slider_stages, slider_drywet;
	CButton m_buttonPhaserEnabled;
	bool m_ownPhaserUpdate;

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

class myElem_t : public  ui_element_impl_withpopup< uielem_phaser > {
	bool get_element_group(pfc::string_base & p_out)
	{
		p_out = "Effect DSP";
		return true;
	}

	bool get_menu_command_description(pfc::string_base & out) {
		out = "Opens a window for phasing effects control.";
		return true;
	}

	bool get_popup_specs(ui_size& defSize, pfc::string_base& title)
	{
		defSize = { 200,240 };
		title = "Phaser DSP";
		return true;
	}

};
static service_factory_single_t<myElem_t> g_myElemFactory;

class CMyDSPPopupPhaser : public CDialogImpl<CMyDSPPopupPhaser>
{
public:
	CMyDSPPopupPhaser(const dsp_preset & initData, dsp_preset_edit_callback & callback) : m_initData(initData), m_callback(callback) { }
	enum { IDD = IDD_PHASER };

	enum
	{
		FreqMin = 1,
		FreqMax = 200,
		FreqRangeTotal = FreqMax - FreqMin,
		StartPhaseMin = 0,
		StartPhaseMax = 360,
		StartPhaseTotal = 360,
		FeedbackMin = -100,
		FeedbackMax = 100,
		FeedbackRangeTotal = 200,
		DepthMin = 0,
		DepthMax = 256,
		DepthRangeTotal = 255,
		StagesMin = 1,
		StagesMax = 24,
		StagesRangeTotal = 24,
		DryWetMin = 0,
		DryWetMax = 256,
		DryWetRangeTotal = 255
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
		freq = 0.4; startphase = 0;
		fb = 0; depth = 100;
		stages = 2; drywet = 128;
		slider_freq.SetPos((double)(10 * freq));
		slider_startphase.SetPos(startphase);
		slider_fb.SetPos(fb);
		slider_depth.SetPos(depth);
		slider_stages.SetPos(stages);
		slider_drywet.SetPos(drywet);
		RefreshLabel(freq, startphase, fb, depth, stages, drywet);
		dsp_preset_impl preset;
		dsp_phaser::make_preset(freq, startphase, fb, depth, stages, drywet, true, preset);
		m_callback.on_preset_changed(preset);
	}


	LRESULT OnEditControlChange(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		if (wParam == 0x1988)
		{
			GetEditText();
		}
		return 0;
	}


	fb2k::CCoreDarkModeHooks m_hooks;
	void DSPConfigChange(dsp_chain_config const & cfg)
	{
		if (m_hWnd != NULL) {
			ApplySettings();
		}
	}

	void ApplySettings()
	{
		dsp_preset_impl preset2;
		if (static_api_ptr_t<dsp_config_manager>()->core_query_dsp(guid_phaser, preset2)) {
			bool enabled;
			dsp_phaser::parse_preset(freq, startphase, fb, depth, stages, drywet, enabled, preset2);
			slider_freq.SetPos((double)(10 * freq));
			slider_startphase.SetPos(startphase);
			slider_fb.SetPos(fb);
			slider_depth.SetPos(depth);
			slider_stages.SetPos(stages);
			slider_drywet.SetPos(drywet);
			RefreshLabel(freq, startphase, fb, depth, stages, drywet);
		}
	}



	void GetEditText()
	{
		bool preset_changed = false;
		CString text, text2, text3, text4, text5, text6;

		freq_edit.GetWindowText(text);
		startphase_edit.GetWindowText(text2);
		fb_edit.GetWindowText(text3);
		depth_edit.GetWindowText(text4);
		stages_edit.GetWindowText(text5);
		drywet_edit.GetWindowText(text6);

		float freq2 = pfc::clip_t<t_float32>(_ttoi(text), FreqMin, FreqMax) / 10.0;

		if (freq_s != text)
		{
			freq = freq2;
			preset_changed = true;
		}

		float startphase2 = pfc::clip_t<t_int32>(_ttoi(text2), 0, StartPhaseMax);

		if (startphase_s != text)
		{
			startphase = startphase2;
			preset_changed = true;
		}

		float fb2 = pfc::clip_t<t_int32>(_ttoi(text3), -100, 100);

		if (fb_s != text)
		{
			fb = fb2;
			preset_changed = true;
		}

		float depth2 = pfc::clip_t<t_float32>(_ttoi(text4), 0, 100);

		if (depth_s != text)
		{
			depth = (depth2 / 100 * 256);
			preset_changed = true;
		}

		float stages2 = pfc::clip_t<t_int32>(_ttoi(text5), StagesMin, StagesMax);

		if (stages_s != text)
		{
			stages = stages2;
			preset_changed = true;
		}

		float drywet2 = pfc::clip_t<t_int32>(_ttoi(text6), 0, 100);

		if (drywet_s != text)
		{
			drywet = (drywet2 / 100 * 256);
			preset_changed = true;
		}

		if (preset_changed)
		{
			slider_freq.SetPos((double)(10 * freq));
			slider_startphase.SetPos(startphase);
			slider_fb.SetPos(fb);
			slider_depth.SetPos(depth);
			slider_stages.SetPos(stages);
			slider_drywet.SetPos(drywet);
			RefreshLabel(freq, startphase, fb, depth, stages, drywet);
			dsp_preset_impl preset;
			dsp_phaser::make_preset(freq, startphase, fb, depth, stages, drywet, true, preset);
			m_callback.on_preset_changed(preset);

		}
	}

	BOOL OnInitDialog(CWindow, LPARAM)
	{
		slider_freq = GetDlgItem(IDC_PHASERSLFOFREQ);
		slider_freq.SetRange(FreqMin, FreqMax);
		slider_startphase = GetDlgItem(IDC_PHASERSLFOSTARTPHASE);
		slider_startphase.SetRange(0, StartPhaseMax);
		slider_fb = GetDlgItem(IDC_PHASERSFEEDBACK);
		slider_fb.SetRange(-100, 100);
		slider_depth = GetDlgItem(IDC_PHASERSDEPTH);
		slider_depth.SetRange(0, 255);
		slider_stages = GetDlgItem(IDC_PHASERSTAGES);
		slider_stages.SetRange(StagesMin, StagesMax);
		slider_drywet = GetDlgItem(IDC_PHASERSDRYWET);
		slider_drywet.SetRange(0, 255);



		freq_edit.AttachToDlgItem(m_hWnd);
		freq_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASERFREQ));
		startphase_edit.AttachToDlgItem(m_hWnd);
		startphase_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASERSTART));
		fb_edit.AttachToDlgItem(m_hWnd);
		fb_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASERFB));
		depth_edit.AttachToDlgItem(m_hWnd);
		depth_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASERDEPTH));
		stages_edit.AttachToDlgItem(m_hWnd);
		stages_edit.SubclassWindow(GetDlgItem(IDC_EDITEPHASERSTAGES));
		drywet_edit.AttachToDlgItem(m_hWnd);
		drywet_edit.SubclassWindow(GetDlgItem(IDC_EDITPHASERDRYWET));

		{
			bool enabled;
			dsp_phaser::parse_preset(freq, startphase, fb, depth, stages, drywet, enabled, m_initData);
			slider_freq.SetPos((double)(10 * freq));
			slider_startphase.SetPos(startphase);
			slider_fb.SetPos(fb);
			slider_depth.SetPos(depth);
			slider_stages.SetPos(stages);
			slider_drywet.SetPos(drywet);
			RefreshLabel(freq, startphase, fb, depth, stages, drywet);

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

		freq = slider_freq.GetPos() / 10.0;
		startphase = slider_startphase.GetPos();
		fb = slider_fb.GetPos();
		depth = slider_depth.GetPos();
		stages = slider_stages.GetPos();
		drywet = slider_drywet.GetPos();
		if (LOWORD(nSBCode) != SB_THUMBTRACK)
		{
			dsp_preset_impl preset;
			dsp_phaser::make_preset(freq, startphase, fb, depth, stages, drywet, true, preset);
			m_callback.on_preset_changed(preset);
		}
		RefreshLabel(freq, startphase, fb, depth, stages, drywet);

	}

	void RefreshLabel(float freq, float startphase, float fb, int depth, int stages, int drywet)
	{
		pfc::string_formatter msg;

		msg << pfc::format_float(freq, 0, 1);
		freq_s = msg.c_str();
		freq_edit.SetWindowText(freq_s);
		msg.reset();


		msg << pfc::format_int(startphase);
		startphase_s = msg.c_str();
		startphase_edit.SetWindowText(startphase_s);
		msg.reset();

		msg << pfc::format_int(fb);
		fb_s = msg.c_str();
		fb_edit.SetWindowText(fb_s);
		msg.reset();
		msg << pfc::format_int(depth * 100 / 256);
		depth_s = msg.c_str();
		depth_edit.SetWindowText(depth_s);
		msg.reset();

		msg << pfc::format_int(stages);
		stages_s = msg.c_str();
		stages_edit.SetWindowText(stages_s);
		msg.reset();

		msg << pfc::format_int(drywet * 100 / 256);
		drywet_s = msg.c_str();
		drywet_edit.SetWindowText(drywet_s);
		msg.reset();
	}

	const dsp_preset & m_initData; // modal dialog so we can reference this caller-owned object.
	dsp_preset_edit_callback & m_callback;
	float freq; //0.1 - 4.0
	float startphase;  //0 - 360
	float fb; //-100 - 100
	int depth;//0-255
	int stages; //2-24
	int drywet; //0-255  
	CEditMod freq_edit, startphase_edit, fb_edit, depth_edit, stages_edit, drywet_edit;
	CString freq_s, startphase_s, fb_s, depth_s, stages_s, drywet_s;
	CTrackBarCtrl slider_freq, slider_startphase, slider_fb, slider_depth, slider_stages, slider_drywet;
};

static void RunConfigPopup(const dsp_preset & p_data, HWND p_parent, dsp_preset_edit_callback & p_callback)
{
	CMyDSPPopupPhaser popup(p_data, p_callback);
	if (popup.DoModal(p_parent) != IDOK) p_callback.on_preset_changed(p_data);
}

static dsp_factory_t<dsp_phaser> g_dsp_phaser_factory;

}