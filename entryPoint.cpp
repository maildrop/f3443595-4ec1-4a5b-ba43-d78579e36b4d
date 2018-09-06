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
        if( getMessageValue == 0 ){ // WM_QUIT が戻ります
          return msg.wParam;
        }else if( getMessageValue == -1 ){ // エラーが発生しました。
          DWORD lastError = ::GetLastError();
          UNREFERENCED_PARAMETER( lastError );
          return 0; // 例外を投げたほうがいいいよね
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

  // ヒープ破壊の検出で直ぐ死ぬように設定
  HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);

  std::locale::global( std::locale("")); // ローケルの設定
  wh::LibraryPathExcludeCurrentWorkingDirectory(); // DLL の読み込み先からカレントワーキングディレクトリを排除

  if( !SUCCEEDED( ::CoInitializeEx( NULL , COINIT_APARTMENTTHREADED) ) ){
    MessageBox( NULL , _T("Component Object Model(COM) ライブラリの初期化に失敗しました。"), _T("ERROR"), MB_OK );
    return 1;
  }


  { // Dwmapi.dll は、KnownDLLs の仲間に無いので、明示的リンクをしてやらないと 意図しないInjection が起きる場合がある。
    wh::ScopedDllFunction<decltype(DwmIsCompositionEnabled)> dwmIsCompositionEnabled(dwmapi_dll,"DwmIsCompositionEnabled");
    wh::ScopedDllFunction<decltype(DwmEnableMMCSS)> dwmEnableMMCSS(dwmapi_dll,"DwmEnableMMCSS") ;
    
    if(! dwmIsCompositionEnabled || ! dwmEnableMMCSS ){ 
      MessageBox( NULL , _T("Dwmapi.dllの読み込みに失敗しました"), _T("ERROR"), MB_OK );
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


    // ウィンドウ クラスを登録する
  const wchar_t CLASS_NAME[]  = L"Sample Window Class";
  WNDCLASS wc = {0};

  wc.lpfnWndProc   = WindowProc;
  wc.hInstance     = hInstance;
  wc.lpszClassName = CLASS_NAME;
  wc.hCursor = LoadCursor( NULL, IDC_ARROW );// WNDCLASS でカーソルの初期化しないと待機カーソルのままになる。

  RegisterClass(&wc);

  // ウィンドウを作成する

  HWND hwnd = CreateWindowEx(0,                              // オプションのウィンドウ スタイル
                             CLASS_NAME,                     // ウィンドウ クラス
                             L"Learn to Program Windows",    // ウィンドウ テキスト
                             WS_OVERLAPPEDWINDOW,            // ウィンドウ スタイル
                             // サイズと位置
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             NULL,       // 親ウィンドウ    
                             NULL,       // メニュー
                             hInstance,  // インスタンス ハンドル
                             NULL        // 追加のアプリケーション データ
                             );
  if (hwnd == NULL){ return 0; }
  ShowWindow(hwnd, nCmdShow); // Window を表示する

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

  wh::PumpMessage()(); // メッセージポンプ
  return 0;
}


struct RenderThread{
  HANDLE eventHandle;
  HANDLE threadHandle;
};

RenderThread beginRenderThread(HWND hwnd);

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){
  static RenderThread renderThread = {0};

  switch( uMsg ){ // 本当は、メッセージクラッカー使うべきなのかな・・・
  case WM_KEYDOWN:
    {
      DWORD threadId = GetThreadId( renderThread.threadHandle );
      // 本当はこれを送るべき 
      PostThreadMessage( threadId, WM_APP_SWITCH_SCREENMODE  ,0 , 0 );
    }
    break;
  case WM_DESTROY:// メインウィンドウが破棄されたら、 PostQuitMessage() で終了をかける。
    {
      PostQuitMessage( 0 );
    }
    break;
  case WM_CREATE:
    VERIFY( PostMessage( hwnd, WM_APP_SUBTHREAD_BEGIN , 0 , 0 ) );

    if( []()->BOOL{ // デスクトップコンポジションが有効になっていれば
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
      
      // ここで使っている DwmSetPresentParameters は、Win9 以降のリリースでは廃止が予定されている
      // DXGI のフリップモデルを使う必要がありますとのこと。
      // ということで却下

      DWM_PRESENT_PARAMETERS dpp={0};
      dpp.cbSize = sizeof( DWM_PRESENT_PARAMETERS );
      dpp.fQueue = FALSE;
      dpp.cRefreshStart = 0;
      dpp.cBuffer = 2;
      dpp.fUseSourceRate = FALSE;
      //dpp.rateSource = info.rateCompose; // ソースのレートをCompose のタイミングにあわせる
      //dpp.cRefreshesPerFrame = 1;
      dpp.eSampling =   DWM_SOURCE_FRAME_SAMPLING_POINT;//DWM_SOURCE_FRAME_SAMPLING_COVERAGE;
      // デスクトップコンポジションが無効になっていると、これはエラーを返す。
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
  case WM_PAINT: // Windowsの描画要求だけども、こちら側では描画しないので、ValidateRect で応答しとく
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

  // 手動イベントハンドル、初期値は非シグナル状態で作成 
  HANDLE eventHandle = CreateEvent( NULL, TRUE , FALSE , NULL );
  // スレッドの生成
  HANDLE threadHandle = 
    ([](HANDLE eventHandle,HWND wndHandle){
      HANDLE retval(NULL); // begin_threadex の戻り値 で、スレッドハンドル
      // これの
      RenderThreadArgument* arg = new RenderThreadArgument();
      RenderThreadArgument arg_value = { wndHandle, eventHandle };
      *arg = arg_value;

      retval = (HANDLE)_beginthreadex( NULL, 0 , [](void* ptr)->unsigned int{
          unsigned int retval(0);
          // これはサブスレッドなので COINIT_MULTITHREADED のマルチスレッドで初期化する。
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
              { // レンダリングスレッドのプライオリティの引き上げを、マルチメディアスケジューラクラスサービスにお願いする。
                DWORD taskIndex(0);
                HANDLE taskHandle(NULL);
                taskHandle = AvSetMmThreadCharacteristics( _T("Games") , &taskIndex );
                renderingthreadloop();
                // レンダリングスレッドのプライオリティを戻す
                VERIFY( AvRevertMmThreadCharacteristics( taskHandle ) );
              }
#else
              renderingthreadloop();
#endif 
            }catch(const _com_error &e){
              // どうにかしようね。

              // e.ErrorMessage() で割り当てられる TCHAR* は オブジェクト e が破棄される際に、開放される。
              // @see http://msdn.microsoft.com/ja-jp/library/vstudio/7db325b0.aspx
              OutputDebugString( e.ErrorMessage() ); 

            }
            // メインウィンドウのスレッドに対して、WM_APP_SUBTHREAD_TERMINATE を発行して、終了を通知する。
            VERIFY( PostMessage( arg->hwnd , WM_APP_SUBTHREAD_TERMINATE , 0 ,0 ) );
            delete ptr;
          }
          CoUninitialize();
          return retval;
        } , arg , CREATE_SUSPENDED , NULL );
      if( retval == NULL ){ // スレッドの生成に失敗
        delete arg; 
      }else{ // スレッドの生成に成功したので。
        ::ResumeThread( retval );  // スレッドの開始を行なう。
      }
      return retval;
    })(eventHandle, hwnd);

  RenderThread retval = {0};
  retval.eventHandle = eventHandle;
  retval.threadHandle = threadHandle;
  return retval;
}

