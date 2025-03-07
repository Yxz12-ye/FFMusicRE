#include <windows.h>
#include <commdlg.h>
#include <string>

char* OpenFileDialog()
{
    OPENFILENAME ofn;       
    static char szFile[260];       
    ZeroMemory(&ofn, sizeof(ofn));
    ZeroMemory(szFile, sizeof(szFile));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL; 
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "any (*.*)\0*.*\0txt (*.txt)\0*.txt\0"; 
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL; 
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;


    if (GetOpenFileName(&ofn))
    {
        // return std::string(ofn.lpstrFile); 
        return szFile;
    }
     return "";
}