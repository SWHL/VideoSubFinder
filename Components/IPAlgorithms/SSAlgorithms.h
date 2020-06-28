                              //SSAlgorithms.h//                                
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

#pragma once

#include "IPAlgorithms.h"
#include "DataTypes.h"
#include "MyClosedFigure.h"
#include "Video.h"
#include <string>

using namespace std;

extern clock_t  g_StartTimeRunSubSearch;
extern int		g_RunSubSearch;

extern int      g_threads; // number of threads
extern int		g_DL;	 //sub frame length
extern double	g_tp;	 //text procent
extern double	g_mtpl;  //min text len (in procent)
extern double	g_veple; //vedges points line error

extern bool g_use_ISA_images_for_search_subtitles;
extern bool g_use_ILA_images_for_search_subtitles;
extern bool g_replace_ISA_by_filtered_version;
extern int g_max_dl_down;
extern int g_max_dl_up;

s64 FastSearchSubtitles(CVideo *pV, s64 Begin, s64 End);

int AnalyseImage(simple_buffer<u8> &Im, simple_buffer<u16> *pImILA, int w, int h);

int CompareTwoSubs(simple_buffer<u8> &Im1, simple_buffer<u16> *pImILA1, simple_buffer<u8> &ImVE11, simple_buffer<u8> &ImVE12, simple_buffer<u8> &Im2, simple_buffer<u16> *pImILA2, simple_buffer<u8> &ImVE2, int w, int h, int W, int H, int ymin);

int DifficultCompareTwoSubs2(simple_buffer<u8> &ImF1, simple_buffer<u16> *pImILA1, simple_buffer<u8> &ImNE11, simple_buffer<u8> &ImNE12, simple_buffer<u8> &ImF2, simple_buffer<u16> *pImILA2, simple_buffer<u8> &ImNE2, int w, int h, int W, int H, int ymin);

int CompareTwoSubsOptimal(simple_buffer<u8> &Im1, simple_buffer<u16> *pImILA1, simple_buffer<u8> &ImVE11, simple_buffer<u8> &ImVE12, simple_buffer<u8> &Im2, simple_buffer<u16> *pImILA2, simple_buffer<u8> &ImVE2, int w, int h, int W, int H, int ymin);

//int DifficultCompareTwoSubs(simple_buffer<int> &ImRGB1, simple_buffer<int> &ImF1, simple_buffer<int> &ImRGB2, simple_buffer<int> &ImF2, int w, int h, int W, int H, int ymin);

int SimpleCombineTwoImages(simple_buffer<int> &Im1, simple_buffer<int> &Im2, int size);

int GetCombinedSquare(simple_buffer<int> &Im1, simple_buffer<int> &Im2, int size);

template <class T>
void AddTwoImages(simple_buffer<T> &Im1, simple_buffer<T> &Im2, simple_buffer<T> &ImRES, int size);
template <class T>
void AddTwoImages(simple_buffer<T> &Im1, simple_buffer<T> &Im2, int size);

void ImBGRToNativeSize(simple_buffer<u8> &ImBGROrig, simple_buffer<u8> &ImBGRRes, int w, int h, int W, int H, int xmin, int xmax, int ymin, int ymax);

string VideoTimeToStr(s64 pos);
s64 GetVideoTime(string time);
s64 GetVideoTime(int minute, int sec, int mili_sec);

wxString GetFileName(wxString FilePath);
wxString GetFileExtension(wxString FilePath);


// W - full image include scale (if is) width
// H - full image include scale (if is) height
template <class T>
void ImToNativeSize(simple_buffer<T>& ImOrig, simple_buffer<T>& ImRes, int w, int h, int W, int H, int xmin, int xmax, int ymin, int ymax)
{
	int i, j, dj, x, y;

	custom_assert(ImRes.m_size >= W * H, "ImToNativeSize: Im.m_size >= W*H");
	memset(ImRes.m_pData, 255, W * H * sizeof(T));

	//can_be_optimized
	i = 0;
	j = ymin * W + xmin;
	dj = W - w;
	for (y = 0; y < h; y++)
	{
		for (x = 0; x < w; x++)
		{
			ImRes[j] = ImOrig[i];
			i++;
			j++;
		}
		j += dj;
	}
}


// W - full image include scale (if is) width
// H - full image include scale (if is) height
template <class T>
void ImToNativeSize(simple_buffer<T>& Im, int w, int h, int W, int H, int xmin, int xmax, int ymin, int ymax)
{
	//can_be_optimized
	simple_buffer<T> ImTMP(Im);
	ImToNativeSize(ImTMP, Im, w, h, W, H, xmin, xmax, ymin, ymax);
}