#include <iomanip>
#include <sstream>
#include <locale>
#include <process.h>
#include <TCHAR.h>
#include <Windows.h>
#include <Objbase.h>
#include <comdef.h>
#include <Dwmapi.h>
#include <Avrt.h>

#include "whVERIFY.hxx"
#include "whDllFunction.hxx"
#include "renderThread.h"

#if ((!(defined( _UNICODE ))) || (!(defined( UNICODE ))))
#error 
#endif /* ((!(defined( _UNICODE ))) || (!(defined( UNICODE ))))*/

namespace wh{
  struct PumpMessage{
    inline BOOL TranslateMessage(const MSG *lpMsg){
      assert( lpMsg != nullptr );
      return ::TranslateMessage( lpMsg );
    }

    inline LRESULT DispatchMessage(const MSG *lpMsg){
      assert( lpMsg != nullptr );
      return ::DispatchMessage( lpMsg );
    }

    inline LRESULT handleMessage(const MSG *lpMsg ){
      assert( lpMsg != nullptr );
      BOOL translateMessageValue = this->TranslateMessage( lpMsg );
      UNREFERENCED_PARAMETER( translateMessageValue );
      LRESULT dispatchMessageValue = this->DispatchMessage( lpMsg );
      return dispatchMessageValue;
    }

    WPARAM operator()(){
      for( ; ; ){
        MSG msg = {0};
        BOOL getMessageValue = GetMessage(&msg, NULL, 0, 0);
        if( getMessageValue == 0 ){ // WM_QUIT ���߂�܂�
          return msg.wParam;
        }else if( getMessageValue == -1 ){ // �G���[���������܂����B
          DWORD lastError = ::GetLastError();
          UNREFERENCED_PARAMETER( lastError );
          return 0; // ��O�𓊂����ق������������
        }else{
          LRESULT lresult = handleMessage( &msg );
          UNREFERENCED_PARAMETER( lresult );
        }
      }
    }
  };
};

static const TCHAR dwmapi_dll[]=_T("Dwmapi.dll");

int WINAPI wWinMain( HINSTANCE , HINSTANCE , PWSTR , int );
int entryPoint( HINSTANCE , HINSTANCE , PWSTR , int );
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

template< typename lambda > 
struct DWMMMCSSEnabler{
  const lambda &enabler;
  DWMMMCSSEnabler(const lambda  &arg): enabler(arg){
    enabler( TRUE );
  }
  ~DWMMMCSSEnabler(){
    //enabler( FALSE );
  }
private:
  DWMMMCSSEnabler& operator=(const DWMMMCSSEnabler &);
};

int WINAPI wWinMain( HINSTANCE hInstance , HINSTANCE prevInstance , PWSTR cmdLine , int nCmdShow ){
  int retval(0);

  // �q�[�v�j��̌��o�Œ������ʂ悤�ɐݒ�
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  std::locale::global( std::locale("")); // ���[�P���̐ݒ�
  wh::LibraryPathExcludeCurrentWorkingDirectory(); // DLL �̓ǂݍ��ݐ悩��J�����g���[�L���O�f�B���N�g����r��

  if( !SUCCEEDED( ::CoInitializeEx( NULL , COINIT_APARTMENTTHREADED) ) ){
    MessageBox( NULL , _T("Component Object Model(COM) ���C�u�����̏������Ɏ��s���܂����B"), _T("ERROR"), MB_OK );
    return 1;
  }


  { // Dwmapi.dll �́AKnownDLLs �̒��Ԃɖ����̂ŁA�����I�����N�����Ă��Ȃ��� �Ӑ}���Ȃ�Injection ���N����ꍇ������B
    wh::ScopedDllFunction<decltype(DwmIsCompositionEnabled)> dwmIsCompositionEnabled(dwmapi_dll,"DwmIsCompositionEnabled");
    wh::ScopedDllFunction<decltype(DwmEnableMMCSS)> dwmEnableMMCSS(dwmapi_dll,"DwmEnableMMCSS") ;
    
    if(! dwmIsCompositionEnabled || ! dwmEnableMMCSS ){ 
      MessageBox( NULL , _T("Dwmapi.dll�̓ǂݍ��݂Ɏ��s���܂���"), _T("ERROR"), MB_OK );
      goto CLEANUP_COM;
    }

    auto enableMMCSS = [&dwmIsCompositionEnabled,&dwmEnableMMCSS]( BOOL fEnableMMCSS ){
      BOOL enabled(FALSE);
      VERIFY( S_OK == dwmIsCompositionEnabled( &enabled ));
      if( enabled ){
        VERIFY( S_OK == dwmEnableMMCSS( fEnableMMCSS ) );
      }
      return;
    };

    {
      DWMMMCSSEnabler<decltype( enableMMCSS )>  enabler(enableMMCSS);
      retval = entryPoint(hInstance, prevInstance, cmdLine, nCmdShow );
    }
  }

 CLEANUP_COM:
  CoUninitialize();
  return retval;
}



int entryPoint( HINSTANCE hInstance , HINSTANCE prevInstance , PWSTR cmdLine , int nCmdShow ){
  UNREFERENCED_PARAMETER( hInstance );
  UNREFERENCED_PARAMETER( prevInstance );
  UNREFERENCED_PARAMETER( cmdLine );
  UNREFERENCED_PARAMETER( nCmdShow );


    // �E�B���h�E �N���X��o�^����
  const wchar_t CLASS_NAME[]  = L"Sample Window Class";
  WNDCLASS wc = {0};

  wc.lpfnWndProc   = WindowProc;
  wc.hInstance     = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hCursor = LoadCursor( NULL, IDC_ARROW );// WNDCLASS �ŃJ�[�\���̏��������Ȃ��Ƒҋ@�J�[�\���̂܂܂ɂȂ�B

  RegisterClass(&wc);

  // �E�B���h�E���쐬����

  HWND hwnd = CreateWindowEx(0,                              // �I�v�V�����̃E�B���h�E �X�^�C��
                             CLASS_NAME,                     // �E�B���h�E �N���X
                             L"Learn to Program Windows",    // �E�B���h�E �e�L�X�g
                             WS_OVERLAPPEDWINDOW,            // �E�B���h�E �X�^�C��
                             // �T�C�Y�ƈʒu
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             NULL,       // �e�E�B���h�E    
                             NULL,       // ���j���[
                             hInstance,  // �C���X�^���X �n���h��
                             NULL        // �ǉ��̃A�v���P�[�V���� �f�[�^
                             );
  if (hwnd == NULL){ return 0; }
  ShowWindow(hwnd, nCmdShow); // Window ��\������

#if 0 
  {
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_DISABLED;
    VERIFY( S_OK == DwmSetWindowAttribute( hwnd,  DWMWA_NCRENDERING_POLICY , &ncrp , sizeof( ncrp ) ));
  }
  {
    DWM_BLURBEHIND  blurBehind = {0};
    blurBehind.dwFlags = DWM_BB_ENABLE;
    blurBehind.fEnable = TRUE;
    ::DwmEnableBlurBehindWindow( hwnd, &blurBehind );
  }
#endif

  wh::PumpMessage()(); // ���b�Z�[�W�|���v
  return 0;
}


struct RenderThread{
  HANDLE eventHandle;
  HANDLE threadHandle;
};

RenderThread beginRenderThread(HWND hwnd);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
  static RenderThread renderThread = {0};

  switch( uMsg ){ // �{���́A���b�Z�[�W�N���b�J�[�g���ׂ��Ȃ̂��ȁE�E�E
  case WM_KEYDOWN:
    {
      DWORD threadId = GetThreadId( renderThread.threadHandle );
      // �{���͂���𑗂�ׂ� 
      PostThreadMessage( threadId, WM_APP_SWITCH_SCREENMODE  ,0 , 0 );
    }
    break;
  case WM_DESTROY:// ���C���E�B���h�E���j�����ꂽ��A PostQuitMessage() �ŏI����������B
    {
      PostQuitMessage( 0 );
    }
    break;
  case WM_CREATE:
    VERIFY( PostMessage( hwnd, WM_APP_SUBTHREAD_BEGIN , 0 , 0 ) );

    if( []()->BOOL{ // �f�X�N�g�b�v�R���|�W�V�������L���ɂȂ��Ă����
        BOOL value(FALSE);
        value = ( S_OK == DwmIsCompositionEnabled( &value ) ) ? value : FALSE;
        return value;  }() ){
      
      DWM_TIMING_INFO info = {0}; 
      info.cbSize = sizeof(DWM_TIMING_INFO);
      VERIFY( S_OK == DwmGetCompositionTimingInfo(NULL, &info) );
      {
        std::wstringstream buffer;
        buffer << L"info.rateRefresh.uiNumerator" << info.rateCompose.uiNumerator << std::endl;
        buffer << L"info.rateRefresh.uiDenominator" << info.rateCompose.uiDenominator << std::endl;
        OutputDebugString( buffer.str().c_str() );
      }
      
      // �����Ŏg���Ă��� DwmSetPresentParameters �́AWin9 �ȍ~�̃����[�X�ł͔p�~���\�肳��Ă���
      // DXGI �̃t���b�v���f�����g���K�v������܂��Ƃ̂��ƁB
      // �Ƃ������Ƃŋp��

      DWM_PRESENT_PARAMETERS dpp={0};
      dpp.cbSize = sizeof( DWM_PRESENT_PARAMETERS );
      dpp.fQueue = FALSE;
      dpp.cRefreshStart = 0;
      dpp.cBuffer = 2;
      dpp.fUseSourceRate = FALSE;
      //dpp.rateSource = info.rateCompose; // �\�[�X�̃��[�g��Compose �̃^�C�~���O�ɂ��킹��
      //dpp.cRefreshesPerFrame = 1;
      dpp.eSampling =   DWM_SOURCE_FRAME_SAMPLING_POINT;//DWM_SOURCE_FRAME_SAMPLING_COVERAGE;
      // �f�X�N�g�b�v�R���|�W�V�����������ɂȂ��Ă���ƁA����̓G���[��Ԃ��B
      VERIFY(S_OK == DwmSetPresentParameters( hwnd , &dpp ));
      //VERIFY(S_OK == DwmSetDxFrameDuration( hwnd, 1 ));

    }
    break;
  case WM_SETCURSOR:
    break;
  case WM_CLOSE:
    return [&]()->LRESULT{
      if( renderThread.eventHandle != NULL){
        VERIFY(::ResetEvent( renderThread.eventHandle ));
        VERIFY(::ShowWindow( hwnd , SW_HIDE ));
        return static_cast<LRESULT>(0);
      }else{
        return DefWindowProc( hwnd, uMsg, wParam , lParam );
      }
    }();
  case WM_PAINT: // Windows�̕`��v�������ǂ��A�����瑤�ł͕`�悵�Ȃ��̂ŁAValidateRect �ŉ������Ƃ�
    VERIFY( ValidateRect( hwnd, NULL ) );
    break;
  case WM_SIZE:
    [](WPARAM wParam , LPARAM lParam ){
      if( renderThread.threadHandle != NULL ){
        DWORD threadId = GetThreadId( renderThread.threadHandle );
        PostThreadMessage( threadId, WM_SIZE , wParam , lParam );
      }
    }( wParam , lParam );
    break;
    /*
  case WM_MOUSEMOVE:
    {
      LRESULT retval = ::DefWindowProc( hwnd, uMsg, wParam , lParam );
      [](WPARAM wParam , LPARAM lParam ){
        DWORD threadId = GetThreadId( renderThread.threadHandle );
        PostThreadMessage( threadId, WM_MOUSEMOVE , wParam , lParam );
      }( wParam , lParam );
      return retval;
    }
    */
  case WM_APP_SUBTHREAD_BEGIN:
    OutputDebugString(_T("WM_APP_SUBTHREAD_BEGIN"));
    return [&](){
      if( renderThread.eventHandle == NULL ){
        renderThread = beginRenderThread( hwnd );
      }
      return static_cast<LRESULT>( 0 );
    }();
  case WM_APP_SUBTHREAD_TERMINATE:
    return [&](){

      if( renderThread.threadHandle == NULL ){

      }else{
        WaitForSingleObject( renderThread.threadHandle , INFINITE );
        CloseHandle( renderThread.threadHandle );
        renderThread.threadHandle = NULL;
      }
      if( renderThread.eventHandle == NULL ){

      }else{
        CloseHandle( renderThread.eventHandle );
        renderThread.eventHandle = NULL;
      }

      VERIFY( DestroyWindow( hwnd ) );
      return 0;
    }();
  default: 
    break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


RenderThread beginRenderThread(HWND hwnd){
  
  struct RenderThreadArgument{
    HWND hwnd;
    HANDLE renderEventHandle;
  };

  // �蓮�C�x���g�n���h���A�����l�͔�V�O�i����Ԃō쐬 
  HANDLE eventHandle = CreateEvent( NULL, TRUE , FALSE , NULL );
  // �X���b�h�̐���
  HANDLE threadHandle = 
    ([](HANDLE eventHandle,HWND wndHandle){
      HANDLE retval(NULL); // begin_threadex �̖߂�l �ŁA�X���b�h�n���h��
      // �����
      RenderThreadArgument* arg = new RenderThreadArgument();
      RenderThreadArgument arg_value = { wndHandle, eventHandle };
      *arg = arg_value;

      retval = (HANDLE)_beginthreadex( NULL, 0 , [](void* ptr)->unsigned int{
          unsigned int retval(0);
          // ����̓T�u�X���b�h�Ȃ̂� COINIT_MULTITHREADED �̃}���`�X���b�h�ŏ���������B
          VERIFY( CoInitializeEx( NULL , COINIT_MULTITHREADED  ) == S_OK );
          if( ptr != nullptr ){
            RenderThreadArgument* arg = static_cast<RenderThreadArgument*>( ptr );
            try{
              
              struct RenderingThreadLoop{
                RenderThreadArgument *arg;
                RenderingThreadLoop( RenderThreadArgument *arg ): arg(arg){};
                inline unsigned int operator()(){
                  unsigned int retval(0);
                  if( arg != nullptr ){
                    while(retval == 0){
                      retval = renderthreadEntryPoint( arg->hwnd, arg->renderEventHandle );
                    }
                  }
                  return retval;
                }
              } renderingthreadloop( arg );
              
#if 1
              { // �����_�����O�X���b�h�̃v���C�I���e�B�̈����グ���A�}���`���f�B�A�X�P�W���[���N���X�T�[�r�X�ɂ��肢����B
                DWORD taskIndex(0);
                HANDLE taskHandle(NULL);
                taskHandle = AvSetMmThreadCharacteristics( _T("Games") , &taskIndex );
                renderingthreadloop();
                // �����_�����O�X���b�h�̃v���C�I���e�B��߂�
                VERIFY( AvRevertMmThreadCharacteristics( taskHandle ) );
              }
#else
              renderingthreadloop();
#endif 
            }catch(const _com_error &e){
              // �ǂ��ɂ����悤�ˁB

              // e.ErrorMessage() �Ŋ��蓖�Ă��� TCHAR* �� �I�u�W�F�N�g e ���j�������ۂɁA�J�������B
              // @see http://msdn.microsoft.com/ja-jp/library/vstudio/7db325b0.aspx
              OutputDebugString( e.ErrorMessage() ); 

            }
            // ���C���E�B���h�E�̃X���b�h�ɑ΂��āAWM_APP_SUBTHREAD_TERMINATE �𔭍s���āA�I����ʒm����B
            VERIFY( PostMessage( arg->hwnd , WM_APP_SUBTHREAD_TERMINATE , 0 ,0 ) );
            delete ptr;
          }
          CoUninitialize();
          return retval;
        } , arg , CREATE_SUSPENDED , NULL );
      if( retval == NULL ){ // �X���b�h�̐����Ɏ��s
        delete arg; 
      }else{ // �X���b�h�̐����ɐ��������̂ŁB
        ::ResumeThread( retval );  // �X���b�h�̊J�n���s�Ȃ��B
      }
      return retval;
    })(eventHandle, hwnd);

  RenderThread retval = {0};
  retval.eventHandle = eventHandle;
  retval.threadHandle = threadHandle;
  return retval;
}

