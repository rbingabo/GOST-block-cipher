#include <windows.h>
#include <windowsX.h>
#include <string> 
#include <iostream>
#include <tchar.h>
#include "resource.h"

using namespace std;
#define IDC_EDIT_ORG	101
#define IDC_BUTTON_MOVE 102
#define IDC_EDIT_DEST 104
#define IDC_BUTTON_CHOOSE 105
#define IDC_EDIT_POLY 108
#define IDC_EDIT_INIT 109
#define IDC_RADIO1 110
#define IDC_RADIO2 111
#define IDC_RADIO3 112
#define IDC_RADIO4 113
#define IDC_RADIO5 114
#define IDC_GROUP1 115
#define IDC_GROUP2 116

unsigned __int64 ivalue;
int len = 1;

const char g_szClassName[] = "myWindowClass";
TCHAR szFileName[MAX_PATH] = "";
TCHAR NewPath[_MAX_PATH] = "";
LPCSTR SelectedPath = NULL;
HANDLE hFileIn;
HANDLE hFileOut;


void DoFileOpen(HWND hwnd)
{
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hwnd;
	ofn.lpstrFilter = TEXT("All Files (*.*)\0*.*\0");
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
	//ofn.lpstrDefExt = "txt";

	if (GetOpenFileName(&ofn))
	{
		HWND hEdit = GetDlgItem(hwnd, IDC_EDIT_ORG);
		SelectedPath = szFileName;
		SetDlgItemText(hwnd, IDC_EDIT_ORG, SelectedPath);

		char original_path[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_tcscpy_s(original_path, SelectedPath);
		_splitpath(original_path, drive, dir, fname, ext);
		_tcscpy_s(NewPath, "");
		_tcscat_s(NewPath, _MAX_PATH, drive);
		_tcscat_s(NewPath, _MAX_PATH, dir);
		_tcscat_s(NewPath, _MAX_PATH, fname);
		_tcscat_s(NewPath, _MAX_PATH, ext);
		_tcscat_s(NewPath, _MAX_PATH, ".crypt");

		SetDlgItemText(hwnd, IDC_EDIT_DEST, NewPath);
	}
}

unsigned __int64 round(unsigned __int64 _block, unsigned __int32 subkey, short s_block[][16])
{
	unsigned __int64 block = 0;
	unsigned __int32 right = _block;
	unsigned __int32 left;
	unsigned __int32 N;
	unsigned __int32 SN = 0;
	unsigned __int32 right1 = right;
	left = _block >> 32;
	N = (right + subkey) % 4294967296;


	for (int j = 0; j <= 7; j++)
	{
		unsigned __int8 Ni = (N >> (4 * (7 - j))) % 16;
		Ni = s_block[j][Ni]; // substitution through s-blocks.

		unsigned __int32 mask = 0;
		mask = mask | Ni;
		mask = mask << (28 - (4 * j));
		SN = SN | mask;
	}
	N = SN;

	unsigned __int32 mask = N << 11;
	N = (N >> 21) | mask;

	right = N^left;
	left = right1;
	block = block | left;
	block = block << 32;
	block = block | right;

	return block;
}
unsigned __int64 encrypt(unsigned __int64 _block, unsigned __int32 *key, short s_block[][16])
{
	unsigned __int64 block = _block;
	unsigned __int32 left = 0;

	for (int k = 1; k <= 3; k++)
	for (int i = 0; i <= 7; i++)
		block = round(block, key[i], s_block);

	for (int i = 7; i >= 0; i--)
		block = round(block, key[i], s_block);

	block = (block << 32) | (block >> 32);

	return block;
}
unsigned __int64 decrypt(unsigned __int64 _block, unsigned __int32 *key, short s_block[][16])
{
	unsigned __int64 block = _block;
	unsigned __int32 left = 0;

	for (int i = 0; i <= 7; i++)
		block = round(block, key[i], s_block);

	for (int k = 1; k <= 3; k++)
	for (int i = 7; i >= 0; i--)
		block = round(block, key[i], s_block);

	block = (block << 32) | (block >> 32);


	return block;
}

void ECB_ENC(unsigned __int32 *key, short s_block[][16])
{
	unsigned __int64 block;
	DWORD dwBytesRead = 0;
	BOOL bResult = FALSE;
	BOOL bFilled = FALSE;
	block = 0;
	while (1)
	{
		block = 0;
		bResult = ReadFile(hFileIn, &block, 8, &dwBytesRead, NULL);

		if (bResult &&  dwBytesRead == 0)
		{
			if (bFilled)
			{
				break;
			}
			else
			{
				block = 0x382a2a2a2a2a2a2a;
				bFilled = TRUE;
			}
		}
		__int8 bytesToFill;
		if (bResult && dwBytesRead < 8 && !bFilled)
		{
			bytesToFill = 8 - dwBytesRead;
			__int64 mask = 0;

			for (int i = dwBytesRead; i < 7; i++)
			{
				char fill = '*';
				mask = 0;
				mask = mask | fill;
				mask = mask << i * 8;
				block = block | mask;
			}
			char ch = bytesToFill + '0';
			mask = 0;
			mask |= ch;
			mask <<= (7 * 8);
			block = block | mask;
			bFilled = TRUE;
		}
		block = encrypt(block, key, s_block);

		WriteFile(hFileOut, &block, 8, NULL, NULL);
	}
	SetEndOfFile(hFileOut);

}
void ECB_DEC(unsigned __int32 *key, short s_block[][16])
{
	unsigned __int64 block;
	DWORD dwBytesRead = 0;
	BOOL bResult = FALSE;
	block = 0;
	int bl = 0;
	while (1)
	{
		block = 0;
		DWORD dwFileSize = GetFileSize(hFileIn, NULL);
		DWORD blocks = dwFileSize / 8;

		bResult = ReadFile(hFileIn, &block, 8, &dwBytesRead, NULL);

		if (bResult &&  dwBytesRead == 0)
		{
			break;
		}

		block = decrypt(block, key, s_block);
		bl++;
		int important;
		char ch1;
		if (bl == blocks)
		{
			ch1 = block >> 56;
			important = 8 - (ch1 - '0');
			WriteFile(hFileOut, &block, important, NULL, NULL);
		}
		else
			WriteFile(hFileOut, &block, 8, NULL, NULL);
	}
	SetEndOfFile(hFileOut);

}

void GAMMI(unsigned __int32 *key, short s_block[][16], unsigned __int64 gamma)
{
	unsigned __int64 block;
	unsigned __int64 S = gamma;
	S = encrypt(S, key, s_block);
	unsigned __int32 S1 = (S >> 32);
	unsigned __int32 S0 = S;
	DWORD dwBytesRead = 0;
	BOOL bResult = FALSE;
	while (1)
	{
		block = 0;
		bResult = ReadFile(hFileIn, &block, 8, &dwBytesRead, NULL);
		if (bResult &&  dwBytesRead == 0)
		{
			break;
		}
		S0 = S0 + 0x1010101;
		S1 = (S1 + 0x1010104 - 1) % (4294967296 - 1) + 1;

		S = S1;
		S = S << 32;
		S |= S0;

		block = block^ encrypt(S, key, s_block);

		WriteFile(hFileOut, &block, 8, NULL, NULL);
	}
	SetEndOfFile(hFileOut);


}

void CFB_ENC(unsigned __int32 *key, short s_block[][16], unsigned __int64 gamma)
{
	unsigned __int64 block;
	unsigned __int64 S = gamma;
	DWORD dwBytesRead = 0;
	BOOL bResult = FALSE;
	while (1)
	{
		block = 0;
		bResult = ReadFile(hFileIn, &block, 8, &dwBytesRead, NULL);
		if (bResult &&  dwBytesRead == 0)
		{
			break;
		}

		block = block ^ encrypt(S, key, s_block);
		S = block;

		WriteFile(hFileOut, &block, 8, NULL, NULL);
	}
	SetEndOfFile(hFileOut);

}
void CFB_DEC(unsigned __int32 *key, short s_block[][16], unsigned __int64 gamma)
{
	unsigned __int64 block;
	unsigned __int64 e_block;
	unsigned __int64 S = gamma;
	DWORD dwBytesRead = 0;
	BOOL bResult = FALSE;
	while (1)
	{
		block = 0;
		bResult = ReadFile(hFileIn, &block, 8, &dwBytesRead, NULL);
		if (bResult &&  dwBytesRead == 0)
		{
			break;
		}
		e_block = block;
		block = block ^ encrypt(S, key, s_block);
		S = e_block;

		WriteFile(hFileOut, &block, 8, NULL, NULL);
	}
	SetEndOfFile(hFileOut);

}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	switch (msg)
	{
	case WM_CREATE:
	{
					  HFONT hfDefault;
					  HWND hEdit, hEdit2, HInit;

					  hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), NULL,
						  WS_CHILD | WS_VISIBLE,
						  10, 30, 582, 30, hwnd, (HMENU)IDC_EDIT_ORG, GetModuleHandle(NULL), NULL);

					  hEdit2 = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), NULL,
						  WS_CHILD | WS_VISIBLE,
						  10, 110, 582, 30, hwnd, (HMENU)IDC_EDIT_DEST, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("1 3 5 6 2 7 9 12"),
						  WS_CHILD | WS_VISIBLE,
						  10, 190, 200, 25, hwnd, (HMENU)IDC_EDIT_POLY, GetModuleHandle(NULL), NULL);

					  HInit = CreateWindowEx(WS_EX_CLIENTEDGE, TEXT("EDIT"), TEXT("170"),
						  WS_CHILD | WS_VISIBLE,
						  260, 190, 180, 25, hwnd, (HMENU)IDC_EDIT_INIT, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("Encrypt / Decrypt"),
						  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 450, 190, 142, 25,
						  hwnd, (HMENU)IDC_BUTTON_MOVE, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Static"), TEXT("Input file"),
						  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 0, 582, 25,
						  hwnd, (HMENU)106, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Static"), TEXT("Output file"),
						  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 80, 582, 25,
						  hwnd, (HMENU)107, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Static"), TEXT("Key\t\t\t\t       Initialization vector"),
						  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 10, 160, 582, 25,
						  hwnd, (HMENU)109, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("Operating mode"),
						  WS_CHILD | BS_GROUPBOX | WS_GROUP | WS_VISIBLE, 10, 220, 140, 100,
						  hwnd, (HMENU)IDC_GROUP1, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("Encryption"),
						  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 20, 250, 90, 25,
						  hwnd, (HMENU)IDC_RADIO1, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("Decryption"),
						  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 20, 280, 90, 25,
						  hwnd, (HMENU)IDC_RADIO2, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("Block cipher mode of operation"),
						  WS_CHILD | BS_GROUPBOX | WS_GROUP | WS_VISIBLE, 200, 220, 240, 115,
						  hwnd, (HMENU)IDC_GROUP2, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("ECB"),
						  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP, 210, 250, 90, 25,
						  hwnd, (HMENU)IDC_RADIO3, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("XOR cipher "),
						  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 210, 280, 90, 25,
						  hwnd, (HMENU)IDC_RADIO4, GetModuleHandle(NULL), NULL);

					  CreateWindowEx(0, TEXT("Button"), TEXT("CFB"),
						  WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON, 210, 310, 90, 25,
						  hwnd, (HMENU)IDC_RADIO5, GetModuleHandle(NULL), NULL);

					  if (hEdit == NULL)
						  MessageBox(hwnd, TEXT("Could not create edit box."), TEXT("Error"), MB_OK | MB_ICONERROR);
					  hfDefault = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
					  SendMessage(hEdit, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE, 0));
					  SendMessage(hEdit2, WM_SETFONT, (WPARAM)hfDefault, MAKELPARAM(FALSE, 0));

					  Button_SetCheck(GetDlgItem(hwnd, IDC_RADIO1), 1);
					  Button_SetCheck(GetDlgItem(hwnd, IDC_RADIO3), 1);
					  Edit_Enable(HInit, FALSE);

	}
		break;


	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_FILE_OPENFILE:
			DoFileOpen(hwnd);
			break;
		case IDC_RADIO3:
		{
						   HWND hInit = GetDlgItem(hwnd, IDC_EDIT_INIT);
						   Edit_Enable(hInit, FALSE);
		}
			break;
		case IDC_RADIO4:
		case IDC_RADIO5:
		{
						   HWND hInit = GetDlgItem(hwnd, IDC_EDIT_INIT);
						   Edit_Enable(hInit, TRUE);
		}
			break;


		case IDC_BUTTON_MOVE:
		{
								LPTSTR polynom = new TCHAR[MAX_PATH];
								LPTSTR init = new TCHAR[10];
								GetDlgItemText(hwnd, IDC_EDIT_POLY, polynom, MAX_PATH);
								GetDlgItemText(hwnd, IDC_EDIT_INIT, init, 10);
								GetDlgItemText(hwnd, IDC_EDIT_DEST, NewPath, MAX_PATH);
								std::string s = polynom;
								std::string delimiter = " ";
								size_t pos = 0;
								std::string token;
								len = 1;
								while ((pos = s.find(delimiter)) != std::string::npos)
								{
									token = s.substr(0, pos);
									s.erase(0, pos + delimiter.length());
									len++;
								}


								hFileIn = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
								if (hFileIn != INVALID_HANDLE_VALUE)
								{

								}
								hFileOut = CreateFile(NewPath, GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_ALWAYS, NULL, NULL);

								len = 8;
								unsigned __int32 key[8];


								int i = 0;
								s = polynom;
								pos = 0;
								while ((pos = s.find(delimiter)) != std::string::npos)
								{
									token = s.substr(0, pos);
									s.erase(0, pos + delimiter.length());
									key[i] = stoi(token);
									i++;
								}

								key[len - 1] = stoi(s);

								ivalue = stoi(init);

								short s_block[8][16] = {
									{ 4, 10, 9, 2, 13, 8, 0, 14, 6, 11, 1, 12, 7, 15, 5, 3 },
									{ 14, 11, 4, 12, 6, 13, 15, 10, 2, 3, 8, 1, 0, 7, 5, 9 },
									{ 5, 8, 1, 13, 10, 3, 4, 2, 14, 15, 12, 7, 6, 0, 9, 11 },
									{ 7, 13, 10, 1, 0, 8, 9, 15, 14, 4, 6, 12, 11, 2, 5, 3 },
									{ 6, 12, 7, 1, 5, 15, 13, 8, 4, 10, 9, 14, 0, 3, 11, 2 },
									{ 4, 11, 10, 0, 7, 2, 1, 13, 3, 6, 8, 5, 9, 12, 15, 14 },
									{ 13, 11, 4, 1, 3, 15, 5, 9, 0, 10, 14, 7, 6, 8, 2, 12 },
									{ 1, 15, 13, 0, 5, 7, 10, 4, 9, 2, 3, 14, 6, 11, 8, 12 }
								};

								if (IsDlgButtonChecked(hwnd, IDC_RADIO1))
								{
									if (IsDlgButtonChecked(hwnd, IDC_RADIO3)) ECB_ENC(key, s_block);
									else if (IsDlgButtonChecked(hwnd, IDC_RADIO4)) GAMMI(key, s_block, ivalue);
									else if (IsDlgButtonChecked(hwnd, IDC_RADIO5)) CFB_ENC(key, s_block, ivalue);

								}
								else if (IsDlgButtonChecked(hwnd, IDC_RADIO2))
								{
									if (IsDlgButtonChecked(hwnd, IDC_RADIO3)) ECB_DEC(key, s_block);
									else if (IsDlgButtonChecked(hwnd, IDC_RADIO4)) GAMMI(key, s_block, ivalue);
									else if (IsDlgButtonChecked(hwnd, IDC_RADIO5))CFB_DEC(key, s_block, ivalue);
								}

								CloseHandle(hFileIn);
								CloseHandle(hFileOut);


		}
		}
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = 0;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
	wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
	wc.lpszClassName = g_szClassName;
	wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	if (!RegisterClassEx(&wc))
	{
		MessageBox(NULL, TEXT("Window Registration Failed!"), TEXT("Error!"),
			MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}

	hwnd = CreateWindowEx(
		0,
		g_szClassName,
		TEXT("GOST ENCRYPTION"),
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 620, 400,
		NULL, NULL, hInstance, NULL);

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while (GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	return Msg.wParam;
}
