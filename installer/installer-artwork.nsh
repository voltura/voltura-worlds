Var ArtworkControl
Var ArtworkPrevious
Var ArtworkSource
Var ArtworkScreen
Var ArtworkSourceDC
Var ArtworkTargetDC
Var ArtworkBitmap
Var ArtworkSourceSelection
Var ArtworkTargetSelection
Var ArtworkWidth
Var ArtworkHeight
Var ArtworkRect
!define ARTWORK_WIDTH 1312
!define ARTWORK_HEIGHT 2512

!macro DefineSmoothArtwork PREFIX
Function ${PREFIX}SmoothArtwork
    Pop $ArtworkControl
    Pop $ArtworkPrevious
    Push $0
    Push $1
    System::Alloc 16
    Pop $ArtworkRect
    StrCmp $ArtworkRect 0 artwork_original
    System::Call 'user32::GetClientRect(p $ArtworkControl, p $ArtworkRect)'
    System::Call '*$ArtworkRect(i, i, i .r0, i .r1)'
    System::Free $ArtworkRect
    StrCpy $ArtworkWidth $0
    StrCpy $ArtworkHeight $1
    IntCmp $ArtworkWidth 0 artwork_original artwork_original
    IntCmp $ArtworkHeight 0 artwork_original artwork_original
    System::Call 'user32::LoadImageW(p 0, w "$PLUGINSDIR\modern-wizard.bmp", i 0, i 0, i 0, i 0x10) p .s'
    Pop $ArtworkSource
    StrCmp $ArtworkSource 0 artwork_original
    System::Call 'user32::GetDC(p $ArtworkControl) p .s'
    Pop $ArtworkScreen
    System::Call 'gdi32::CreateCompatibleDC(p $ArtworkScreen) p .s'
    Pop $ArtworkSourceDC
    System::Call 'gdi32::CreateCompatibleDC(p $ArtworkScreen) p .s'
    Pop $ArtworkTargetDC
    System::Call 'gdi32::CreateCompatibleBitmap(p $ArtworkScreen, i $ArtworkWidth, i $ArtworkHeight) p .s'
    Pop $ArtworkBitmap
    StrCmp $ArtworkBitmap 0 artwork_cleanup
    System::Call 'gdi32::SelectObject(p $ArtworkSourceDC, p $ArtworkSource) p .s'
    Pop $ArtworkSourceSelection
    System::Call 'gdi32::SelectObject(p $ArtworkTargetDC, p $ArtworkBitmap) p .s'
    Pop $ArtworkTargetSelection
    ; LoadImage's default resize aliases small lettering. Halftone samples the full-resolution artwork.
    System::Call 'gdi32::SetStretchBltMode(p $ArtworkTargetDC, i 4)'
    System::Call 'gdi32::SetBrushOrgEx(p $ArtworkTargetDC, i 0, i 0, p 0)'
    System::Call 'gdi32::StretchBlt(p $ArtworkTargetDC, i 0, i 0, i $ArtworkWidth, i $ArtworkHeight, p $ArtworkSourceDC, i 0, i 0, i ${ARTWORK_WIDTH}, i ${ARTWORK_HEIGHT}, i 0x00CC0020) i .r0'
    System::Call 'gdi32::SelectObject(p $ArtworkSourceDC, p $ArtworkSourceSelection)'
    System::Call 'gdi32::SelectObject(p $ArtworkTargetDC, p $ArtworkTargetSelection)'
    StrCmp $0 0 artwork_failed
    SendMessage $ArtworkControl ${STM_SETIMAGE} 0 $ArtworkBitmap
    System::Call 'gdi32::DeleteObject(p $ArtworkPrevious)'
    StrCpy $ArtworkPrevious $ArtworkBitmap
    Goto artwork_cleanup
    artwork_failed:
        System::Call 'gdi32::DeleteObject(p $ArtworkBitmap)'
    artwork_cleanup:
        System::Call 'gdi32::DeleteDC(p $ArtworkSourceDC)'
        System::Call 'gdi32::DeleteDC(p $ArtworkTargetDC)'
        System::Call 'user32::ReleaseDC(p $ArtworkControl, p $ArtworkScreen)'
        System::Call 'gdi32::DeleteObject(p $ArtworkSource)'
    artwork_original:
        Pop $1
        Pop $0
        Push $ArtworkPrevious
FunctionEnd

!if "${PREFIX}" == ""
Function ${PREFIX}SmoothWelcomeArtwork
    Push $mui.WelcomePage.Image.Bitmap
    Push $mui.WelcomePage.Image
    Call ${PREFIX}SmoothArtwork
    Pop $mui.WelcomePage.Image.Bitmap
FunctionEnd
!endif

Function ${PREFIX}SmoothFinishArtwork
    Push $mui.FinishPage.Image.Bitmap
    Push $mui.FinishPage.Image
    Call ${PREFIX}SmoothArtwork
    Pop $mui.FinishPage.Image.Bitmap
FunctionEnd
!macroend
