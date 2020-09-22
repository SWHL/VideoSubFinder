                              //OCRPanel.cpp//                                
//////////////////////////////////////////////////////////////////////////////////
//																				//
// Author:  Simeon Kosnitsky													//
//          skosnits@gmail.com													//
//																				//
// License:																		//
//     This software is released into the public domain.  You are free to use	//
//     it in any way you like, except that you may not sell this source code.	//
//																				//
//     This software is provided "as is" with no expressed or implied warranty.	//
//     I accept no liability for any damage or loss of business that this		//
//     software may cause.														//
//																				//
//////////////////////////////////////////////////////////////////////////////////

#include "OCRPanel.h"
#include "CommonFunctions.h"
#include <algorithm>
#include <vector>
#include <regex>
#include <fstream>
#include <streambuf>
#include <ppl.h>
#include <ppltasks.h>
#include <concurrent_queue.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <wx/regex.h>
#include <wx/sound.h>
using namespace std;

bool g_use_ISA_images_for_get_txt_area = true;
bool g_use_ILA_images_for_get_txt_area = true;

bool g_join_subs_and_correct_time = true;
bool g_clear_txt_folders = true;

int g_IsCreateClearedTextImages = 0;
int g_RunCreateClearedTextImages = 0;
bool g_ValidateAndCompareTXTImages = false;
bool g_DontDeleteUnrecognizedImages1 = true;
bool g_DontDeleteUnrecognizedImages2 = true;

wxString g_DefStringForEmptySub = "sub duration: %sub_duration%";

bool g_CLEAN_RGB_IMAGES = false;

int  g_ocr_threads = 8;

AssTXTLine::AssTXTLine()
{
	m_LH = 0;
	m_LY = 0;
	m_LXB = 0;
	m_LXE = 0;
	m_LYB = 0;
	m_LYE = 0;

	m_mY = 0;
	m_mI = 0;
	m_mQ = 0;

	m_BT = 0;
	m_ET = 0;

	m_pAssStyle = NULL;

	m_dX = -1;
	m_dY = -1;
	m_Alignment = -1;
}

AssTXTLine& AssTXTLine::operator=(const AssTXTLine &other)
{
	m_TXTStr = other.m_TXTStr;
	m_LH = other.m_LH;
	m_LY = other.m_LY;
	m_LXB = other.m_LXB;
	m_LXE = other.m_LXE;
	m_LYB = other.m_LYB;
	m_LYE = other.m_LYE;

	m_mY = other.m_mY;
	m_mI = other.m_mI;
	m_mQ = other.m_mQ;

	m_BT = other.m_BT;
	m_ET = other.m_ET;

	m_pAssStyle = other.m_pAssStyle;

	m_dX = other.m_dX;
	m_dY = other.m_dY;
	m_Alignment = other.m_Alignment;

	return *this;
}

AssTXTStyle::AssTXTStyle()
{	
	m_data.clear();

	m_minY = 0;
	m_minI = 0;
	m_minQ = 0;
	
	m_maxY = 0;
	m_maxI = 0;
	m_maxQ = 0;

	m_mY = 0;
	m_mI = 0;
	m_mQ = 0;

	m_minLH = 0;
	m_maxLH = 0;

	m_LH = 0;

	m_Alignment = 2;
	m_MarginL = 10;
	m_MarginR = 10;
	m_MarginV = 10;
}

// W - full image include scale (if is) width
// H - full image include scale (if is) height
void AssTXTStyle::Compute(int W, int H)
{
	int i;
	int size, val1, val2, val3, val4;

	val1 = 0;
	val2 = 0;
	val3 = 0;
	val4 = 0;

	size = (int)m_data.size();

	for (i=0; i<size; i++)
	{
		val1 += m_data[i].m_mY;
		val2 += m_data[i].m_mI;
		val3 += m_data[i].m_mQ;
		val4 += m_data[i].m_LH;
	}
	

	m_mY = val1/size;
	m_mI = val2/size;
	m_mQ = val3/size;
	m_LH = (val4*528*100)/(size*H*53);
	m_LH += m_LH%2;
}

wxString AssSubHead =
"[Script Info]\n\
; Script generated by VideoSubFinder\n\
; http://www.aegisub.org/\n\
Title: Default Aegisub file\n\
ScriptType: v4.00+\n\
WrapStyle: 0\n\
ScaledBorderAndShadow: yes\n\
YCbCr Matrix: None\n\
\n\
[Aegisub Project Garbage]\n\
\n\
[V4+ Styles]\n\
Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, MarginL, MarginR, MarginV, Encoding\n\
Style: Default,Arial,20,&H00FFFFFF,&H000000FF,&H00000000,&H00000000,0,0,0,0,100,100,0,0,1,2,2,2,10,10,10,1\n\
\n\
[Events]\n\
Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text\n";

BEGIN_EVENT_TABLE(COCRPanel, wxPanel)
	EVT_BUTTON(ID_BTN_CES, COCRPanel::OnBnClickedCreateEmptySub)
	EVT_BUTTON(ID_BTN_CSCTI, COCRPanel::OnBnClickedCreateSubFromClearedTXTImages)
	EVT_BUTTON(ID_BTN_CSTXT, COCRPanel::OnBnClickedCreateSubFromTXTResults)
	EVT_BUTTON(ID_BTN_CCTI, COCRPanel::OnBnClickedCreateClearedTextImages)
END_EVENT_TABLE()

COCRPanel::COCRPanel(CSSOWnd* pParent)
		:wxPanel( pParent, wxID_ANY )
{
	m_pParent = pParent;
	m_pMF = pParent->m_pMF;
}

COCRPanel::~COCRPanel()
{
}

void COCRPanel::Init()
{
	SaveToReportLog("COCRPanel::Init(): starting...\n");

	m_CL1 = wxColour(255, 215, 0);
	m_CLOCR = wxColour(170, 170, 170);

	//"Times New Roman"
	SaveToReportLog("COCRPanel::Init(): init m_BTNFont...\n");
	m_BTNFont = wxFont(m_pMF->m_cfg.m_fount_size_ocr_btn, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL,
                    wxFONTWEIGHT_BOLD, false /* !underlined */,
                    wxEmptyString /* facename */, wxFONTENCODING_DEFAULT);

	//"Microsoft Sans Serif"
	SaveToReportLog("COCRPanel::Init(): init m_LBLFont...\n");
	m_LBLFont = wxFont(m_pMF->m_cfg.m_fount_size_ocr_lbl, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                    wxFONTWEIGHT_NORMAL, false /* !underlined */,
                    wxEmptyString /* facename */, wxFONTENCODING_DEFAULT);


	wxRect rcCCTI, rcCES, rcP3, rcClP3, rlMSD, reMSD, rlJSACT, rlCTXTF, rlSESS, rlSSI, rcTEST, rcCSCTI, rcCSTXT;
	int w, w2, h, dw, dh, txt_dw = m_pMF->m_cfg.m_txt_dw, txt_dy = m_pMF->m_cfg.m_txt_dy;

	wxClientDC dc(this);
	dc.SetFont(m_BTNFont);
	wxSize min_ocr_btn_size = get_max_wxSize({ dc.GetTextExtent(m_pMF->m_cfg.m_ocr_button_ces_text),
									dc.GetTextExtent(m_pMF->m_cfg.m_ocr_button_ccti_text),
									dc.GetTextExtent(m_pMF->m_cfg.m_ocr_button_csftr_text),
									dc.GetTextExtent(m_pMF->m_cfg.m_ocr_button_cesfcti_text) });
	w2 = 700;
	w = min_ocr_btn_size.x + txt_dw*2;
	h = min_ocr_btn_size.y + txt_dy*2;

	rcCCTI.x = w2/2 - w/2;
	rcCCTI.y = 20;
	rcCCTI.width = w;
	rcCCTI.height = h;

	rcCSTXT.x = rcCCTI.x;
	rcCSTXT.y = rcCCTI.GetBottom() + 10;
	rcCSTXT.width = w;
	rcCSTXT.height = h;

	rcCSCTI.x = rcCCTI.x;
	rcCSCTI.y = rcCSTXT.GetBottom() + 10;
	rcCSCTI.width = w;
	rcCSCTI.height = h;

	rcCES.x = rcCCTI.x;
	rcCES.y = rcCSCTI.GetBottom() + 10;
	rcCES.width = w;
	rcCES.height = h;

	rcTEST.x = rcCCTI.GetRight() + 30;
	rcTEST.y = rcCCTI.GetBottom() + 5 - h/2;
	rcTEST.width = 100;
	rcTEST.height = h;

	rlMSD.x = 10;
	rlMSD.y = 20;
	rlMSD.width = rcCCTI.GetLeft() - rlMSD.x*2;
	rlMSD.height = 18;

	int cb_dist = 6;

	reMSD.x = rlMSD.x;
	reMSD.y = rlMSD.GetBottom() + cb_dist;
	reMSD.width = rlMSD.width;
	reMSD.height = 18;

	rlJSACT.x = reMSD.x;
	rlJSACT.y = reMSD.GetBottom() + cb_dist;
	rlJSACT.width = reMSD.width;
	rlJSACT.height = 18;

	rlCTXTF.x = rlJSACT.x;
	rlCTXTF.y = rlJSACT.GetBottom() + cb_dist;
	rlCTXTF.width = rlJSACT.width;
	rlCTXTF.height = 18;

	rlSESS.x = rlCTXTF.x;
	rlSESS.y = rlCTXTF.GetBottom() + cb_dist;
	rlSESS.width = rlCTXTF.width;
	rlSESS.height = 18;
	
	rlSSI.x = rlSESS.x;
	rlSSI.y = rlSESS.GetBottom() + cb_dist;
	rlSSI.width = rlSESS.width;
	rlSSI.height = 18;

	rcP3 = this->GetRect();

	this->GetClientSize(&w, &h);
	rcClP3.x = rcClP3.y = 0; 
	rcClP3.width = w;
	rcClP3.height = h;

	dw = rcP3.width - rcClP3.width;
	dh = rcP3.height - rcClP3.height;

	rcP3.x = 10;	
	rcP3.y = 10;
	rcP3.width = w2 + dw;
	rcP3.height = rcCES.GetBottom() + 20 + dh;

	SaveToReportLog("COCRPanel::Init(): this->SetSize(rcP3)...\n");
	this->SetSize(rcP3);	

	SaveToReportLog("COCRPanel::Init(): init m_pP3...\n");
	m_pP3 = new wxPanel( this, wxID_ANY, rcP3.GetPosition(), rcP3.GetSize() );
	m_pP3->SetMinSize(rcP3.GetSize());
	m_pP3->SetBackgroundColour( m_CLOCR );

	SaveToReportLog("COCRPanel::Init(): init m_plblMSD...\n");
	m_plblMSD = new wxStaticText( m_pP3, wxID_ANY,
		m_pMF->m_cfg.m_ocr_label_msd_text, rlMSD.GetPosition(), rlMSD.GetSize(), wxALIGN_LEFT | wxST_NO_AUTORESIZE | wxBORDER);
	m_plblMSD->SetFont(m_LBLFont);
	m_plblMSD->SetBackgroundColour( m_CL1 );

	SaveToReportLog("COCRPanel::Init(): init m_pMSD...\n");
	m_pMSD = new CTextCtrl(m_pP3, wxID_ANY,
		&(m_pMF->m_cfg.m_ocr_min_sub_duration), reMSD.GetPosition(), reMSD.GetSize());
	m_pMSD->SetFont(m_LBLFont);

	SaveToReportLog("COCRPanel::Init(): init m_pcbJSACT...\n");
	m_pcbJSACT = new CCheckBox(m_pP3, wxID_ANY, &g_join_subs_and_correct_time,
		m_pMF->m_cfg.m_ocr_label_jsact_text, rlJSACT.GetPosition(), rlJSACT.GetSize(), wxALIGN_RIGHT | wxST_NO_AUTORESIZE | wxBORDER);
	m_pcbJSACT->SetFont(m_LBLFont);
	m_pcbJSACT->SetBackgroundColour(m_CL1);

	SaveToReportLog("COCRPanel::Init(): init m_pcbCTXTF...\n");
	m_pcbCTXTF = new CCheckBox(m_pP3, wxID_ANY, &g_clear_txt_folders,
		m_pMF->m_cfg.m_ocr_label_clear_txt_folders, rlCTXTF.GetPosition(), rlCTXTF.GetSize(), wxALIGN_RIGHT | wxST_NO_AUTORESIZE | wxBORDER);
	m_pcbCTXTF->SetFont(m_LBLFont);
	m_pcbCTXTF->SetBackgroundColour(m_CL1);

	SaveToReportLog("COCRPanel::Init(): init m_pcbSESS...\n");
	m_pcbSESS = new CCheckBox(m_pP3, wxID_ANY, &g_save_each_substring_separately,
		m_pMF->m_cfg.m_ocr_label_save_each_substring_separately, rlSESS.GetPosition(), rlSESS.GetSize(), wxALIGN_RIGHT | wxST_NO_AUTORESIZE | wxBORDER);
	m_pcbSESS->SetFont(m_LBLFont);
	m_pcbSESS->SetBackgroundColour(m_CL1);

	SaveToReportLog("COCRPanel::Init(): init m_pcbSSI...\n");
	m_pcbSSI = new CCheckBox(m_pP3, wxID_ANY, &g_save_scaled_images,
		m_pMF->m_cfg.m_ocr_label_save_scaled_images, rlSSI.GetPosition(), rlSSI.GetSize(), wxALIGN_RIGHT | wxST_NO_AUTORESIZE | wxBORDER);
	m_pcbSSI->SetFont(m_LBLFont);
	m_pcbSSI->SetBackgroundColour(m_CL1);

	SaveToReportLog("COCRPanel::Init(): init m_pCES...\n");
	m_pCES = new wxButton( m_pP3, ID_BTN_CES,
		m_pMF->m_cfg.m_ocr_button_ces_text, rcCES.GetPosition(), rcCES.GetSize());
	m_pCES->SetFont(m_BTNFont);

	SaveToReportLog("COCRPanel::Init(): init m_pCCTI...\n");
	m_pCCTI = new wxButton( m_pP3, ID_BTN_CCTI,
		m_pMF->m_cfg.m_ocr_button_ccti_text, rcCCTI.GetPosition(), rcCCTI.GetSize());
	m_pCCTI->SetFont(m_BTNFont);

	SaveToReportLog("COCRPanel::Init(): init m_pCSTXT...\n");
	m_pCSTXT = new wxButton( m_pP3, ID_BTN_CSTXT,
		m_pMF->m_cfg.m_ocr_button_csftr_text, rcCSTXT.GetPosition(), rcCSTXT.GetSize());
	m_pCSTXT->SetFont(m_BTNFont);

	SaveToReportLog("COCRPanel::Init(): init m_pCSCTI...\n");
	m_pCSCTI = new wxButton( m_pP3, ID_BTN_CSCTI,
		m_pMF->m_cfg.m_ocr_button_cesfcti_text, rcCSCTI.GetPosition(), rcCSCTI.GetSize());
	m_pCSCTI->SetFont(m_BTNFont);

	wxBoxSizer *top_sizer = new wxBoxSizer( wxVERTICAL );

	wxBoxSizer *button_sizer = new wxBoxSizer( wxHORIZONTAL );

	button_sizer->Add(m_pP3, 1, wxALIGN_CENTER, 0 );

	top_sizer->Add(button_sizer, 1, wxALIGN_CENTER );

	SaveToReportLog("COCRPanel::Init(): this->SetSizer(top_sizer)...\n");
	this->SetSizer(top_sizer);

	SaveToReportLog("COCRPanel::Init(): finished.\n");
}

void COCRPanel::OnBnClickedCreateEmptySub(wxCommandEvent& event)
{
	wxString Str, SubStr, hour1, hour2, min1, min2, sec1, sec2, msec1, msec2;
	int i, j, k, sec, msec;
	u64 bt, et, dt, mdt;
	wxString str_int;

	wxDir dir(g_work_dir + "/RGBImages");
	vector<wxString> FileNamesVector;
	vector<u64> BT, ET;
	wxString filename;
	bool res;

	res = dir.GetFirst(&filename);
    while ( res )
    {
		FileNamesVector.push_back(filename);

        res = dir.GetNext(&filename);
    }
	
	for (i=0; i<(int)FileNamesVector.size()-1; i++)
	for (j=i+1; j<(int)FileNamesVector.size(); j++)
	{
		if (FileNamesVector[i] > FileNamesVector[j])
		{
			Str = FileNamesVector[i];
			FileNamesVector[i] = FileNamesVector[j];
			FileNamesVector[j] = Str;
		}
	}	

	mdt = (s64)(m_pMF->m_cfg.m_ocr_min_sub_duration * (double)1000);

	for(k=0; k<(int)FileNamesVector.size(); k++)
	{
		Str = FileNamesVector[k];

		hour1 = Str.Mid(0,1);
		min1 = Str.Mid(2,2);
		sec1 = Str.Mid(5,2);
		msec1 = Str.Mid(8,3);

		hour2 = Str.Mid(13,1);
		min2 = Str.Mid(15,2);
		sec2 = Str.Mid(18,2);
		msec2 = Str.Mid(21,3);

		bt = (wxAtoi(hour1)*3600 + wxAtoi(min1)*60 + wxAtoi(sec1))*1000 + wxAtoi(msec1);
		et = (wxAtoi(hour2)*3600 + wxAtoi(min2)*60 + wxAtoi(sec2))*1000 + wxAtoi(msec2);

		BT.push_back(bt);
		ET.push_back(et);
	}

	if (g_join_subs_and_correct_time)
	{
		for (k = 0; k < (int)FileNamesVector.size() - 1; k++)
		{
			if (ET[k] - BT[k] < mdt)
			{
				if (BT[k] + mdt < BT[k + 1])
				{
					ET[k] = BT[k] + mdt;
				}
				else
				{
					ET[k] = BT[k + 1] - 1;
				}
			}
		}
	}	

	wxString srt_sub;
	for(k=0; k<(int)FileNamesVector.size(); k++)
	{
		bt = BT[k];
		et = ET[k];

		Str = VideoTimeToStr2(bt)+
			  " --> "+
			  VideoTimeToStr2(et);

		dt = et - bt;
		sec = (int)(dt/1000);
		msec = (int)(dt%1000);
		
		sec1 = wxString::Format(wxT("%i"), sec);

		str_int = wxString::Format(wxT("%i"), msec);
		if (msec < 10) msec1 = wxT("00") + str_int;
		else
		{
			if (msec < 100) msec1 = wxT("0")+ str_int;
			else msec1 = str_int;
		}

		SubStr = g_DefStringForEmptySub;

		if (g_DefStringForEmptySub.Contains("%sub_duration%"))
		{			
			SubStr.Replace("%sub_duration%", sec1 + "," + msec1);
		}

		srt_sub << (k+1) << wxT("\n") << Str << wxT("\n") << SubStr << "\n\n";
	}

	wxString ass_sub;
	ass_sub << AssSubHead;
	for (k = 0; k < (int)FileNamesVector.size(); k++)
	{
		bt = BT[k];
		et = ET[k];

		dt = et - bt;
		sec = (int)(dt / 1000);
		msec = (int)(dt % 1000);

		sec1 = wxString::Format(wxT("%i"), sec);

		str_int = wxString::Format(wxT("%i"), msec);
		if (msec < 10) msec1 = wxT("00") + str_int;
		else
		{
			if (msec < 100) msec1 = wxT("0") + str_int;
			else msec1 = str_int;
		}

		SubStr = g_DefStringForEmptySub;

		if (g_DefStringForEmptySub.Contains("%sub_duration%"))
		{
			SubStr.Replace("%sub_duration%", sec1 + "," + msec1);
		}

		ass_sub << "Dialogue: 0," + VideoTimeToStr3(bt) + "," + VideoTimeToStr3(et) + ",Default,,0,0,0,," + SubStr + wxT("\n");
	}
	
	SaveSub(srt_sub, ass_sub);
}

void COCRPanel::OnBnClickedCreateSubFromTXTResults(wxCommandEvent& event)
{
	CreateSubFromTXTResults();
}

void COCRPanel::OnBnClickedCreateSubFromClearedTXTImages(wxCommandEvent& event)
{
	wxString Str, SubStr, Name, hour1, hour2, min1, min2, sec1, sec2, msec1, msec2;
	int i, j, k, kb, sec, msec;
	wxString str_int;
	u64 bt, et, dt, mdt;

	wxString dir_path = wxString(g_work_dir + wxT("/TXTImages/"));
	wxDir dir(dir_path);
	vector<wxString> FileNamesVector;
	vector<u64> BT, ET;
	wxString filename;
	bool res;

	res = dir.GetFirst(&filename);
    while ( res )
    {
		FileNamesVector.push_back(filename);

        res = dir.GetNext(&filename);
    }

	for (i=0; i<(int)FileNamesVector.size()-1; i++)
	for (j=i+1; j<(int)FileNamesVector.size(); j++)
	{
		if (FileNamesVector[i] > FileNamesVector[j])
		{
			Str = FileNamesVector[i];
			FileNamesVector[i] = FileNamesVector[j];
			FileNamesVector[j] = Str;
		}
	}

	mdt = (s64)(m_pMF->m_cfg.m_ocr_min_sub_duration * (double)1000);

	k = 0;
	while (k < (int)FileNamesVector.size())
	{
		kb = k;
		i = 0;

		if (g_join_subs_and_correct_time)
		{
			while ((k < (int)FileNamesVector.size()) &&
				(FileNamesVector[kb].Mid(0, 11) == FileNamesVector[k].Mid(0, 11))
				)
			{
				k++;
			}
		}
		else
		{
			k++;
		}

		Str = FileNamesVector[kb];

		hour1 = Str.Mid(0,1);
		min1 = Str.Mid(2,2);
		sec1 = Str.Mid(5,2);
		msec1 = Str.Mid(8,3);

		hour2 = Str.Mid(13,1);
		min2 = Str.Mid(15,2);
		sec2 = Str.Mid(18,2);
		msec2 = Str.Mid(21,3);

		bt = (wxAtoi(hour1)*3600 + wxAtoi(min1)*60 + wxAtoi(sec1))*1000 + wxAtoi(msec1);
		et = (wxAtoi(hour2)*3600 + wxAtoi(min2)*60 + wxAtoi(sec2))*1000 + wxAtoi(msec2);

		BT.push_back(bt);
		ET.push_back(et);
	}

	if (g_join_subs_and_correct_time)
	{
		for (k = 0; k < (int)BT.size() - 1; k++)
		{
			if (ET[k] - BT[k] < mdt)
			{
				if (BT[k] + mdt < BT[k + 1])
				{
					ET[k] = BT[k] + mdt;
				}
				else
				{
					ET[k] = BT[k + 1] - 1;
				}
			}
		}
	}

	wxString srt_sub;
	for(k=0; k<(int)BT.size(); k++)
	{
		bt = BT[k];
		et = ET[k];

		Str = VideoTimeToStr2(bt)+
			  " --> "+
			  VideoTimeToStr2(et);

		dt = et - bt;
		sec = (int)(dt/1000);
		msec = (int)(dt%1000);
		
		sec1 = wxString::Format(wxT("%i"), sec);

		str_int = wxString::Format(wxT("%i"), msec);
		if (msec < 10) msec1 = wxT("00") + str_int;
		else
		{
			if (msec < 100) msec1 = wxT("0") + str_int;
			else msec1 = str_int;
		}

		SubStr = g_DefStringForEmptySub;

		if (g_DefStringForEmptySub.Contains("%sub_duration%"))
		{			
			SubStr.Replace("%sub_duration%", sec1 + "," + msec1);
		}

		srt_sub << (k+1) << wxT("\n") << Str << wxT("\n") << SubStr << "\n\n";
	}

	wxString ass_sub;
	ass_sub << AssSubHead;
	for (k = 0; k < (int)BT.size(); k++)
	{
		bt = BT[k];
		et = ET[k];

		dt = et - bt;
		sec = (int)(dt / 1000);
		msec = (int)(dt % 1000);

		sec1 = wxString::Format(wxT("%i"), sec);

		str_int = wxString::Format(wxT("%i"), msec);
		if (msec < 10) msec1 = wxT("00") + str_int;
		else
		{
			if (msec < 100) msec1 = wxT("0") + str_int;
			else msec1 = str_int;
		}

		SubStr = g_DefStringForEmptySub;

		if (g_DefStringForEmptySub.Contains("%sub_duration%"))
		{
			SubStr.Replace("%sub_duration%", sec1 + "," + msec1);
		}

		ass_sub << "Dialogue: 0," + VideoTimeToStr3(bt) + "," + VideoTimeToStr3(et) + ",Default,,0,0,0,," + SubStr + wxT("\n");
	}

	SaveSub(srt_sub, ass_sub);
}

void COCRPanel::SaveSub(wxString srt_sub, wxString ass_sub)
{
	if (!(m_pMF->m_blnNoGUI))
	{
		m_sub_path.Clear();
		wxFileDialog fd(m_pMF, wxT("Save subtitle as..."),
			g_work_dir, "sub", wxT("SubRip(*.srt)|*.srt|Advanced Sub Station Alpha(*.ass)|*.ass"), wxFD_SAVE);
		int res = fd.ShowModal();

		if (res == wxID_OK)
		{
			m_sub_path = fd.GetPath();
		}
	}
	
	if (m_sub_path.size() > 0)
	{
		wxString ext = GetFileExtension(m_sub_path);

		if (ext == wxT("srt"))
		{
			wxFFileOutputStream ffout(m_sub_path);
			wxTextOutputStream fout(ffout);
			fout << srt_sub;
			fout.Flush();
			ffout.Close();
		}
		else if (ext == wxT("ass"))
		{
			wxFFileOutputStream ffout(m_sub_path);
			wxTextOutputStream fout(ffout);
			fout << ass_sub;
			fout.Flush();
			ffout.Close();
		}
		else
		{
			wxMessageBox("Only .ass and .srt output subtitles formats are supported", "Error", wxOK, this);
		}
	}
}

void COCRPanel::CreateSubFromTXTResults()
{
	wxString Str, Name, hour1, hour2, min1, min2, sec1, sec2, msec1, msec2;
	int i, j, k, kb, sec, msec, max_mY_dif, max_mI_dif, max_mQ_dif, max_posY_dif;
	int val1, val2, val3, val4, val5, val6, val7, val8;
	wxString fname, image_name;
	u64 bt, et, dt, mdt;
	double max_LH_dif;
	int bln;

	vector<wxString> FileNamesVector;
	vector<wxString> TXTVector;
	vector<u64> BT, ET;	

	wxString dir_path = wxString(g_work_dir + wxT("/TXTResults/"));
	wxDir dir(dir_path);
	wxString filename;
	bool res;

	res = dir.GetFirst(&filename, "*.txt");
    while ( res )
    {
		FileNamesVector.push_back(filename);

        res = dir.GetNext(&filename);
    }

	if (FileNamesVector.size() == 0) return;

	for (i=0; i<(int)FileNamesVector.size()-1; i++)
	for (j=i+1; j<(int)FileNamesVector.size(); j++)
	{
		if (FileNamesVector[i] > FileNamesVector[j])
		{
			Str = FileNamesVector[i];
			FileNamesVector[i] = FileNamesVector[j];
			FileNamesVector[j] = Str;
		}
	}

	mdt = (s64)(m_pMF->m_cfg.m_ocr_min_sub_duration * (double)1000);
	
	int W, H;
    
	k = 0;
	while (k < (int)FileNamesVector.size())
	{
		kb = k;

		Str = FileNamesVector[kb];

		hour1 = Str.Mid(0,1);
		min1 = Str.Mid(2,2);
		sec1 = Str.Mid(5,2);
		msec1 = Str.Mid(8,3);

		hour2 = Str.Mid(13,1);
		min2 = Str.Mid(15,2);
		sec2 = Str.Mid(18,2);
		msec2 = Str.Mid(21,3);

		bt = (wxAtoi(hour1)*3600 + wxAtoi(min1)*60 + wxAtoi(sec1))*1000 + wxAtoi(msec1);
		et = (wxAtoi(hour2)*3600 + wxAtoi(min2)*60 + wxAtoi(sec2))*1000 + wxAtoi(msec2);

		BT.push_back(bt);
		ET.push_back(et);

		Str = "";
		i = 0;
		while( (k < (int)FileNamesVector.size()) &&
			   (FileNamesVector[kb].Mid(0, 11) == FileNamesVector[k].Mid(0, 11))
			 )
		{
			Name = g_work_dir + wxT("/TXTResults/") + FileNamesVector[k];

			wxFileInputStream ffin(Name);
			wxTextInputStream fin(ffin, wxT("\x09"), wxConvUTF8);
			wxString str;

			while (ffin.IsOk() && !ffin.Eof())
			{
				str += fin.ReadLine();
				if (ffin.IsOk() && !ffin.Eof())
				{
					str += wxT("\n");
				}
			}
			
			if (str.size() > 0)
			{
				if (i > 0) Str += wxT("\n");
				Str += str;
				i++;
			}

			k++;

			if (!g_join_subs_and_correct_time)
			{
				break;
			}
		}

		TXTVector.push_back(Str);
	}

	// ������� srt subtitle
	
	k=0;
	while(k < (int)TXTVector.size()-1)
	{
		if (TXTVector[k] == "")
		{
			if (g_DontDeleteUnrecognizedImages2 == false)
			{
				if (g_join_subs_and_correct_time)
				{
					for (i = k; i < (int)TXTVector.size() - 1; i++)
					{
						BT[i] = BT[i + 1];
						ET[i] = ET[i + 1];
						TXTVector[i] = TXTVector[i + 1];
					}
					BT.pop_back();
					ET.pop_back();
					TXTVector.pop_back();

					continue;
				}
			}
			else
			{
				TXTVector[k] = wxT("#unrecognized text#");
			}
		}

		if (g_join_subs_and_correct_time)
		{
			if (BT[k + 1] - ET[k] <= 333)
			{
				if (TXTVector[k + 1] == TXTVector[k])
				{
					ET[k] = ET[k + 1];

					for (i = k + 1; i < (int)TXTVector.size() - 1; i++)
					{
						BT[i] = BT[i + 1];
						ET[i] = ET[i + 1];
						TXTVector[i] = TXTVector[i + 1];
					}
					BT.pop_back();
					ET.pop_back();
					TXTVector.pop_back();

					continue;
				}
			}
		}

		k++;
	}

	if (g_join_subs_and_correct_time)
	{
		for (k = 0; k < (int)TXTVector.size() - 1; k++)
		{
			if (ET[k] - BT[k] < mdt)
			{
				if (BT[k] + mdt < BT[k + 1])
				{
					ET[k] = BT[k] + mdt;
				}
				else
				{
					ET[k] = BT[k + 1] - 1;
				}
			}
		}
	}

	wxString srt_sub;
	for(k=0; k<(int)TXTVector.size(); k++)
	{
		bt = BT[k];
		et = ET[k];

		Str = VideoTimeToStr2(bt)+
			  " --> "+
			  VideoTimeToStr2(et);

		srt_sub << (k+1) << wxT("\n") << Str << wxT("\n") << TXTVector[k] << wxT("\n\n");
	}

	wxString ass_sub;
	ass_sub << AssSubHead;
	for (k = 0; k < (int)TXTVector.size(); k++)
	{
		bt = BT[k];
		et = ET[k];

		//example: Dialogue: 0,0:00:03.29,0:00:05.00,Default,,0,0,0,,Regulars gather up!

		wxString txt = TXTVector[k];
		wxRegEx re(wxT("\n"));

		re.ReplaceAll(&txt, wxT("\\\\N"));

		ass_sub << wxT("Dialogue: 0,") + VideoTimeToStr3(bt) + wxT(",") + VideoTimeToStr3(et) + wxT(",Default,,0,0,0,,") + txt + wxT("\n");
	}

	SaveSub(srt_sub, ass_sub);
}

void COCRPanel::OnBnClickedCreateClearedTextImages(wxCommandEvent& event)
{
	if (g_IsCreateClearedTextImages == 0)
	{
		g_IsCreateClearedTextImages = 1;
		g_RunCreateClearedTextImages = 1;

		if (!(m_pMF->m_blnNoGUI))
		{
			m_pCCTI->SetLabel("Stop CCTXTImages");

			if (m_pMF->m_VIsOpen)
			{
				wxCommandEvent event;
				m_pMF->OnStop(event);

				m_pMF->m_VIsOpen = false;

				if (m_pMF->m_timer.IsRunning())
				{
					m_pMF->m_timer.Stop();
				}

				m_pMF->m_ct = -1;				

				m_pMF->m_pVideoBox->m_pVBar->ToggleTool(ID_TB_RUN, false);
				m_pMF->m_pVideoBox->m_pVBar->ToggleTool(ID_TB_PAUSE, false);
				m_pMF->m_pVideoBox->m_pVBar->ToggleTool(ID_TB_STOP, false);				
				m_pMF->m_pImageBox->ClearScreen();
				m_pMF->m_pVideo->SetNullRender();
			}

			m_pMF->m_pVideoBox->m_plblVB->SetLabel("VideoBox");
			m_pMF->m_pVideoBox->m_plblTIME->SetLabel("");

			m_pMF->m_pPanel->m_pSSPanel->Disable();
			m_pMF->m_pPanel->m_pSHPanel->Disable();
		}

		m_pSearchThread = new ThreadCreateClearedTextImages(m_pMF, m_pMF->m_blnNoGUI ? wxTHREAD_JOINABLE : wxTHREAD_DETACHED);
		m_pSearchThread->Create();
		m_pSearchThread->Run();
	}
	else
	{
		m_pMF->m_pPanel->m_pOCRPanel->Disable();
		g_RunCreateClearedTextImages = 0;
	}
}

ThreadCreateClearedTextImages::ThreadCreateClearedTextImages(CMainFrame *pMF, wxThreadKind kind)
        : wxThread(kind)
{
    m_pMF = pMF;
}

class FindTextLinesRes
{
public:
	int m_res = 0;
	int m_w = 0;
	int m_h = 0;
	int	m_W = 0;
	int	m_H = 0;
	int	m_xmin = 0;
	int	m_ymin = 0;
	int	m_xmax = 0;
	int	m_ymax = 0;

	vector<wxString> m_SavedFiles;
	simple_buffer<u8> m_ImBGR;
	simple_buffer<u8> m_ImClearedText;	

	FindTextLinesRes()
	{
	}
};

void FindTextLinesWithExcFilter(FindTextLinesRes *res, simple_buffer<u8>* pImF, simple_buffer<u8>* pImNF, simple_buffer<u8>* pImNE, simple_buffer<u8>* pImIL)
{
	__try
	{		
		res->m_res = FindTextLines(res->m_ImBGR, res->m_ImClearedText, *pImF, *pImNF, *pImNE, *pImIL, res->m_SavedFiles, res->m_w, res->m_h, res->m_W, res->m_H, res->m_xmin, res->m_ymin);
	}
	__except (exception_filter(GetExceptionCode(), GetExceptionInformation(), "got error in FindTextLinesWithExcFilter"))
	{
		res->m_res = -1;
	}
}

void FindTextLines(wxString FileName, FindTextLinesRes &res)
{
	try
	{
		wxString Str, BaseImgName;
		int w, h, W, H, w2, h2, xmin, xmax, ymin, ymax;

		GetImageSize(FileName, w, h);
		GetImInfo(GetFileName(FileName), w, h, &W, &H, &xmin, &xmax, &ymin, &ymax, &BaseImgName);

		res.m_w = w;
		res.m_h = h;
		res.m_W = W;
		res.m_H = H;
		res.m_xmin = xmin;
		res.m_ymin = ymin;
		res.m_xmax = xmax;
		res.m_ymax = ymax;
		res.m_ImBGR = simple_buffer<u8>(w * h * 3, 0);
		res.m_ImClearedText = simple_buffer<u8>(w * h, 0);

		LoadBGRImage(res.m_ImBGR, FileName);

		simple_buffer<u8> ImFF(w * h)/*3*/, ImTF(w * h)/*5*/, ImNE(w * h)/*1*/, ImIL/*0*/;

		{
			simple_buffer<u8> ImSF(w * h)/*4*/, ImY(w * h)/*2*/;			
			res.m_res = GetTransformedImage(res.m_ImBGR, ImFF, ImSF, ImTF, ImNE, ImY, w, h, W, H, 0, w - 1);
		}

		if (g_show_transformed_images_only)
		{
			Str = FileName;
			Str = GetFileName(Str);
			Str = "/TXTImages/" + Str + g_im_save_format;
			SaveGreyscaleImage(ImTF, wxString(Str), w, h);
			res.m_ImClearedText = ImTF;
			return;
		}

		if (g_use_ISA_images_for_get_txt_area)
		{
			Str = g_work_dir + "/ISAImages/" + GetFileName(FileName) + g_im_save_format;

			if (wxFileExists(Str))
			{
				GetImageSize(Str, w2, h2);

				if ( (h2 == ((h*w2)/w)) && (h2 <= h) )
				{
					LoadBinaryImage(ImTF, wxString(Str), w2, h2);
					if (g_show_results) SaveGreyscaleImage(ImTF, "/TestImages/ThreadCreateClearedTextImages_01_01_ISAImage" + g_im_save_format, w2, h2);

					if (h2 != h)
					{
						cv::Mat cv_ImGROrig, cv_ImGR;
						GreyscaleImageToMat(ImTF, w2, h2, cv_ImGROrig);
						cv::resize(cv_ImGROrig, cv_ImGR, cv::Size(0, 0), (double)w/w2, (double)h/h2);
						BinaryMatToImage(cv_ImGR, w, h, ImTF, (u8)255);
						if (g_show_results) SaveGreyscaleImage(ImTF, "/TestImages/ThreadCreateClearedTextImages_01_02_ISAImageScaled" + g_im_save_format, w, h);
					}

					RestoreStillExistLines(ImTF, ImFF, w, h);
					ExtendImFWithDataFromImNF(ImTF, ImFF, w, h);
					if (g_show_results) SaveGreyscaleImage(ImTF, "/TestImages/ThreadCreateClearedTextImages_02_ISAImageExtImNF" + g_im_save_format, w, h);
				}
				else
				{
					g_pMF->ShowErrorMessage(wxString::Format(wxT("ISA Image \"%s\" has wrong size"), GetFileName(FileName) + g_im_save_format));

					if (g_RunCreateClearedTextImages)
					{
						g_pMF->m_pPanel->m_pOCRPanel->Disable();
						g_RunCreateClearedTextImages = 0;
					}
					return;
				}
			}
		}

		// IL image	
		if (g_use_ILA_images_for_get_txt_area)
		{
			Str = g_work_dir + "/ILAImages/" + GetFileName(FileName) + g_im_save_format;

			if (wxFileExists(Str))
			{
				GetImageSize(Str, w2, h2);
				
				if ((h2 == ((h * w2) / w)) && (h2 <= h))
				{
					ImIL.set_size(w * h);
					LoadBinaryImage(ImIL, wxString(Str), w2, h2);
					if (g_show_results) SaveGreyscaleImage(ImIL, "/TestImages/ThreadCreateClearedTextImages_03_01_ILAImage" + g_im_save_format, w2, h2);

					if (h2 != h)
					{
						cv::Mat cv_ImGROrig, cv_ImGR;
						GreyscaleImageToMat(ImIL, w2, h2, cv_ImGROrig);
						cv::resize(cv_ImGROrig, cv_ImGR, cv::Size(0, 0), (double)w / w2, (double)h / h2);
						BinaryMatToImage(cv_ImGR, w, h, ImIL, (u8)255);
						if (g_show_results) SaveGreyscaleImage(ImIL, "/TestImages/ThreadCreateClearedTextImages_03_02_ILAImageScaled" + g_im_save_format, w, h);
					}

					if (g_show_results) SaveGreyscaleImage(ImTF, "/TestImages/ThreadCreateClearedTextImages_04_ISAImage" + g_im_save_format, w, h);
					IntersectTwoImages(ImTF, ImIL, w, h);
					if (g_show_results) SaveGreyscaleImage(ImTF, "/TestImages/ThreadCreateClearedTextImages_05_ISAImageIntILAImage" + g_im_save_format, w, h);

					if (g_show_results) SaveGreyscaleImage(ImFF, "/TestImages/ThreadCreateClearedTextImages_06_ImNF" + g_im_save_format, w, h);
					IntersectTwoImages(ImFF, ImIL, w, h);
					if (g_show_results) SaveGreyscaleImage(ImFF, "/TestImages/ThreadCreateClearedTextImages_07_ImNFIntILAImage" + g_im_save_format, w, h);
				}
				else
				{
					g_pMF->ShowErrorMessage(wxString::Format(wxT("ILA Image \"%s\" has wrong size"), GetFileName(FileName) + g_im_save_format));
					if (g_RunCreateClearedTextImages)
					{
						g_pMF->m_pPanel->m_pOCRPanel->Disable();
						g_RunCreateClearedTextImages = 0;
					}
					return;
				}
			}			
		}		

		res.m_SavedFiles.push_back(BaseImgName);

		FindTextLinesWithExcFilter(&res, &ImTF, &ImFF, &ImNE, &ImIL);

		if (res.m_res == -1)
		{
			g_pMF->SaveError(wxT("Got C Exception during FindTextLinesWithExcFilter on FileName: ") + FileName);
		}

		// free memory for reduce usage
		if (g_pMF->m_blnNoGUI)
		{
			res.m_ImBGR.set_size(0);
			res.m_ImClearedText.set_size(0);
		}
	}
	catch (const exception& e)
	{
		g_pMF->SaveError(wxT("Got C++ Exception: got error in FindTextLines: ") + wxString(e.what()));
	}
}

struct find_text_queue_data
{
	wxString m_file_name;
	FindTextLinesRes* m_p_res;
	my_event* m_p_event;
	bool m_is_end = false;
};

concurrency::task<void> TaskFindTextLines(concurrency::concurrent_queue<find_text_queue_data> &task_queue)
{
	return concurrency::create_task([&task_queue] {
			find_text_queue_data text_queue_data;

			while (1)
			{			
				if (task_queue.try_pop(text_queue_data))
				{
					if (text_queue_data.m_is_end)
					{
						break;
					}
					else
					{
						custom_set_started(text_queue_data.m_p_event);

						if (g_RunCreateClearedTextImages == 0)
						{
							text_queue_data.m_p_event->m_need_to_skip = true;
						}
						else
						{
							FindTextLines(g_work_dir + "/RGBImages/" + text_queue_data.m_file_name, *text_queue_data.m_p_res);															
						}

						text_queue_data.m_p_event->set();
					}
				}
			}
		}
	);
}

s64 getTotalSystemMemory()
{
	MEMORYSTATUSEX status;
	status.dwLength = sizeof(status);
	GlobalMemoryStatusEx(&status);
	return status.ullTotalPhys;
}

void *ThreadCreateClearedTextImages::Entry()
{
	g_IsCreateClearedTextImages = 1;
	
	g_text_alignment = ConvertStringToTextAlignment(g_text_alignment_string);

	if (g_ocr_threads <= 0)
	{
		g_ocr_threads = std::thread::hardware_concurrency();

#ifdef WIN64
		double max_mem = (double)getTotalSystemMemory()/(1 << 30);
		double one_thr_max_mem = (double)13/16; // MAX ~10.7Gb in 16 thread
		int max_thrs = std::max<int>((int)(max_mem / one_thr_max_mem), 1);
		if (g_ocr_threads > max_thrs)
		{
			g_ocr_threads = max_thrs;
		}
#else
		if (g_ocr_threads > 1)
		{
			g_ocr_threads = 1;
		}
#endif
	}

#ifdef WIN32
	// Create a scheduler policy that allows up to g_ocr_threads simultaneous tasks.
	concurrency::CurrentScheduler::Create(concurrency::SchedulerPolicy(2, Concurrency::MinConcurrency, 1, Concurrency::MaxConcurrency, g_ocr_threads));
#endif

	wxString Str, dStr;
	wxString fname;
	ofstream fout;
	char str[30];
	int i, j, k, xmin, xmax, ymin, ymax, val;
	
	int w1, h1, w2, h2, YB1, YB2, bln;
	wxString hour1, hour2, min1, min2, sec1, sec2, msec1, msec2;
	u64 bt1, et1, bt2, et2;

	int res;	

	if (g_clear_txt_folders)
	{
		m_pMF->ClearDir(g_work_dir + "/TXTImages");
		m_pMF->ClearDir(g_work_dir + "/TXTResults");
	}

	wxDir dir(g_work_dir + "/RGBImages");
	vector<wxString> FileNamesVector;
	vector<wxString> prevSavedFiles;
	vector<u64> BT, ET;
	wxString filename;
	bool bres;

	bres = dir.GetFirst(&filename);
    while ( bres )
    {
		FileNamesVector.push_back(filename);

        bres = dir.GetNext(&filename);
    }

	for (i=0; i<(int)FileNamesVector.size()-1; i++)
	for (j=i+1; j<(int)FileNamesVector.size(); j++)
	{
		if (FileNamesVector[i] > FileNamesVector[j])
		{
			Str = FileNamesVector[i];
			FileNamesVector[i] = FileNamesVector[j];
			FileNamesVector[j] = Str;
		}
	}

	__int64 t1, dt, num_calls;

	//t1 = GetTickCount();	

	if (g_clear_test_images_folder) m_pMF->ClearDir(g_work_dir + "/TestImages");
		
	int NImages = FileNamesVector.size();

	if (NImages > 0)
	{
		concurrency::concurrent_queue<find_text_queue_data> task_queue;
		simple_buffer<FindTextLinesRes*> task_results(NImages);
		simple_buffer<my_event*> task_events(NImages);
		vector<concurrency::task<void>> tasks(g_ocr_threads, concurrency::create_task([] {}));		

		if (!(m_pMF->m_blnNoGUI))
		{
			m_pMF->m_pVideoBox->m_pSB->Enable(true);
			m_pMF->m_pVideoBox->m_pSB->SetScrollPos(0);
			m_pMF->m_pVideoBox->m_pSB->SetScrollRange(0, NImages);

			if (m_pMF->m_pVideoBox->m_pImage != NULL)
			{
				delete m_pMF->m_pVideoBox->m_pImage;
				m_pMF->m_pVideoBox->m_pImage = NULL;
			}
			m_pMF->m_pVideoBox->ClearScreen();

			if (m_pMF->m_pImageBox->m_pImage != NULL)
			{
				delete m_pMF->m_pImageBox->m_pImage;
				m_pMF->m_pImageBox->m_pImage = NULL;
			}
			m_pMF->m_pImageBox->ClearScreen();

			wxString str;
			str.Printf(wxT("progress: 0%%   |   0 : %.5d   "), NImages);
			m_pMF->m_pVideoBox->m_plblTIME->SetLabel(str);
		}

		for (k = 0; k < NImages; k++)
		{
			task_results[k] = new FindTextLinesRes();
			task_events[k] = new my_event();
			task_queue.push({ FileNamesVector[k], task_results[k], task_events[k] });
		}

		for (k = 0; k < g_ocr_threads; k++)
		{
			task_queue.push({ "", NULL, NULL, true });
			tasks[k] = TaskFindTextLines(task_queue);
		}

		clock_t start_time = clock();

		for (k = 0; k < NImages; k++)
		{
			try
			{
				task_events[k]->wait();

				if (task_events[k]->m_need_to_skip)
				{
					delete task_events[k];
					delete task_results[k];
					continue;
				}

				FindTextLinesRes* p_task_res = task_results[k];

				res = p_task_res->m_res;


				if (!(m_pMF->m_blnNoGUI))
				{
					{
						simple_buffer<u8> ImTMP_BGR(p_task_res->m_W * p_task_res->m_H * 3);
						ImBGRToNativeSize(p_task_res->m_ImBGR, ImTMP_BGR, p_task_res->m_w, p_task_res->m_h, p_task_res->m_W, p_task_res->m_H, p_task_res->m_xmin, p_task_res->m_xmax, p_task_res->m_ymin, p_task_res->m_ymax);
						g_pViewBGRImage[0](ImTMP_BGR, p_task_res->m_W, p_task_res->m_H);
					}

					{
						simple_buffer<u8> ImTMP_ClearedText(p_task_res->m_W * p_task_res->m_H);
						ImToNativeSize(p_task_res->m_ImClearedText, ImTMP_ClearedText, p_task_res->m_w, p_task_res->m_h, p_task_res->m_W, p_task_res->m_H, p_task_res->m_xmin, p_task_res->m_xmax, p_task_res->m_ymin, p_task_res->m_ymax);
						g_pViewGreyscaleImage[1](ImTMP_ClearedText, p_task_res->m_W, p_task_res->m_H);
					}

					clock_t cur_time = clock();
					double progress = ((double)(k + 1) / (double)NImages) * 100.0;

					clock_t run_time = cur_time - start_time;
					clock_t eta = (clock_t)((double)run_time * (100.0 - progress) / progress);

					wxString str;
					str.Printf(wxT("progress: %%%2.2f eta : %s run_time : %s   |   %.5d : %.5d   "), progress, m_pMF->ConvertClockTime(eta), m_pMF->ConvertClockTime(run_time), k + 1, NImages);
					m_pMF->m_pVideoBox->m_plblTIME->SetLabel(str);

					Str = FileNamesVector[k];
					Str = GetFileName(Str);

					m_pMF->m_pVideoBox->m_plblVB->SetLabel("VideoBox \"" + Str + "\"");

					m_pMF->m_pVideoBox->m_pSB->SetScrollPos(k + 1);

					if (res == -1)
					{
						g_pMF->ShowErrorMessage(wxT("Got C Exception during FindTextLinesWithExcFilter on FileName: ") + FileNamesVector[k]);
					}
				}

				if ((res == 0) && (g_DontDeleteUnrecognizedImages1 == true))
				{
					Str = FileNamesVector[k];
					Str = GetFileName(Str);
					Str = wxT("/TXTImages/") + Str + wxT("_00001") + g_im_save_format;

					simple_buffer<u8> ImRES1((int)(p_task_res->m_w * g_scale) * (int)(p_task_res->m_h / g_scale), 255);
					SaveGreyscaleImage(ImRES1, wxString(Str), p_task_res->m_w * g_scale, p_task_res->m_h / g_scale);
				}

				delete task_events[k];
				delete task_results[k];

				prevSavedFiles = p_task_res->m_SavedFiles;
			}
			catch (const exception& e)
			{
				g_pMF->SaveError(wxT("Got C++ Exception: got error in ThreadCreateClearedTextImages: ") + wxString(e.what()));
			}
		}

		concurrency::when_all(begin(tasks), end(tasks)).wait();
	}

	//(void)wxMessageBox("dt: " + std::to_string(GetTickCount() - t1));

	if (!(m_pMF->m_blnNoGUI))
	{
		m_pMF->m_pPanel->m_pOCRPanel->m_pCCTI->SetLabel("Create Cleared TXT Images");

		m_pMF->m_pPanel->m_pSHPanel->Enable();
		m_pMF->m_pPanel->m_pSSPanel->Enable();		
		m_pMF->m_pPanel->m_pOCRPanel->Enable();

		if ((g_RunCreateClearedTextImages == 1) && g_playback_sound)
		{
			Str = g_app_dir + wxT("/finished.wav");
			if (wxFileExists(Str))
			{
				wxSound sound(Str);
				if (sound.IsOk())
				{
					sound.Play();
				}
			}
		}
	}

	g_IsCreateClearedTextImages = 0;
	
	return 0;
}
