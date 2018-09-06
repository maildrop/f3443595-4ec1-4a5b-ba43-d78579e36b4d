#define _USE_MATH_DEFINES // for M_PI 
#define NOMINMAX // Windows.h が min max をマクロ定義しているので、 math.h の min/max と競合するのを殺す
#include <sstream>
#include <cassert>
#include <cmath>
#include <comip.h>
#include <TCHAR.h>
#include <Windows.h>
#include "whVERIFY.hxx"
#include <D3D10_1.h>
#include <d2d1.h> 
#include <Dwrite.h>
#include <Dwmapi.h>
#include "renderThread.h"

/*
  swapChain の バックバッファ から、ID2D1Factory::CreateDxgiSurfaceRenderTarget を使用して、
  ID2D1RenderTarget を生成する。
  生成に失敗した場合は、nullptr が戻る。
*/

static ID2D1RenderTarget* createRenderTarget(IDXGISwapChain *swapChain,ID2D1Factory *d2dfactory){
  assert( swapChain != nullptr );
  assert( d2dfactory != nullptr );

  _com_ptr_t<_com_IIID< IDXGISurface , &__uuidof( IDXGISurface ) > > dxgiSurface([](IDXGISwapChain *swapChain){
      IDXGISurface *backBufferSurface(nullptr);
      HRESULT hr = swapChain->GetBuffer( 0 , IID_PPV_ARGS( &backBufferSurface ) );
      return (hr==S_OK) ? backBufferSurface : nullptr ;
    }( swapChain ) , false );
  if(! static_cast<bool>(dxgiSurface) ){ // @see _com_ptr_t::operator bool() 
    return nullptr;
  }

  ID2D1RenderTarget *renderTarget(nullptr); // 戻り値
  D2D1_RENDER_TARGET_PROPERTIES props = [](ID2D1Factory* d2dfactory){
    FLOAT dpiX(0.0f);
    FLOAT dpiY(0.0f);
    d2dfactory->GetDesktopDpi( & dpiX , &dpiY );
    return D2D1::RenderTargetProperties( D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                         D2D1::PixelFormat( DXGI_FORMAT_UNKNOWN , D2D1_ALPHA_MODE_PREMULTIPLIED),
                                         dpiX, dpiY );
  }(d2dfactory);
  HRESULT hr = d2dfactory->CreateDxgiSurfaceRenderTarget( static_cast<IDXGISurface*>(dxgiSurface) , props, &renderTarget );
  return (hr == S_OK) ? renderTarget : nullptr;
};

/*
  DXGIのスクリーンモード切り替えは、メインスレッドのメッセージフックで行われるので、切り替え自体がメインスレッドでおきる。
  問題になるのは、IDXGISurface の同期の問題である。

  メインスレッドから切り替えが行われると、 IDXGISwapChain のバックバッファの再設定が行われ、
  このスレッドで保持している IDXGISurface が無効になる。IDXGISurface が無効になるということは
  レンダリングターゲットも無効になる。
  レンダリングスレッドと ウィンドウのメッセージポンプスレッドが分離している場合、
  このような理由によりMakeWindowAssociation で alt+enter の全画面表示への切り替えは禁止する。
*/

unsigned int renderthreadEntryPoint(HWND hWnd,HANDLE continueHandle){
  // ウィンドウモード
  static BOOL windowMode = TRUE;

  unsigned int retval(0);
  assert( hWnd != NULL );
  assert( continueHandle != NULL );

  if( continueHandle == NULL ){
    return 1;
  }
  // スレッドメッセージキューを生成して、 continueHandle のイベントを立てておく
  [](HANDLE continueHandle){ 
    // PeekMessage するとスレッドメッセージキューが生成される。
    MSG msg = {0};
    PeekMessage( &msg, NULL , WM_USER , WM_USER , PM_NOREMOVE ); 
    // メッセージキューが生成されたので、イベントを立ち上げる。
    SetEvent(continueHandle);
  }(continueHandle);


  // デバイス非依存部分の初期化

  // ID2D1Factory のインターフェースポインタ（のスマートポインタ）
  _com_ptr_t<_com_IIID<ID2D1Factory, &__uuidof(ID2D1Factory)> > pD2DFactory([](){
      ID2D1Factory *factory(nullptr);
      HRESULT hr;
      hr = D2D1CreateFactory( D2D1_FACTORY_TYPE_MULTI_THREADED , &factory );
      return (hr==S_OK) ? factory : nullptr ;
    }(), false );
  if( pD2DFactory.GetInterfacePtr() == nullptr ){  return 1;  }

  // DXGIFactory1 のインターフェースポインタ
  _com_ptr_t<_com_IIID<IDXGIFactory1, &__uuidof(IDXGIFactory1)> > pDXGIFactory([](){
      IDXGIFactory1 *ptr(nullptr);
      HRESULT hr = CreateDXGIFactory1( __uuidof( IDXGIFactory1 ), reinterpret_cast<void**>(&ptr) );
      return ( hr == S_OK )? ptr : nullptr;
    }(), false );
  if( pDXGIFactory.GetInterfacePtr() == nullptr ){  return 1;  }
  // 別のスレッドから直接 DXGI の状態を変えられると非常にまずいので、これは、禁止にしておく
  pDXGIFactory->MakeWindowAssociation( NULL , DXGI_MWA_NO_WINDOW_CHANGES ); 
  
  // D3D10Device1 の生成
  _com_ptr_t<_com_IIID<ID3D10Device1,&__uuidof( ID3D10Device1 ) > > pD3D10Device1([](IDXGIFactory1* dxgifactory,UINT flags){
      assert( dxgifactory != nullptr );

      ID3D10Device1 *device(nullptr);
      static const D3D10_DRIVER_TYPE driverTypes[] = {D3D10_DRIVER_TYPE_HARDWARE, D3D10_DRIVER_TYPE_WARP};
      static const D3D10_FEATURE_LEVEL1 levelAttempts[] =
        {  D3D10_FEATURE_LEVEL_10_1,
           D3D10_FEATURE_LEVEL_10_0,
           D3D10_FEATURE_LEVEL_9_3,
           D3D10_FEATURE_LEVEL_9_2,
           D3D10_FEATURE_LEVEL_9_1  };
      for( auto driverType : driverTypes ){
        for( auto levelAttempt : levelAttempts ){
          // 下の goto 文のコメントに対応する 防衛的 assert 文 
          // ここに到達するときは device は nullptr であるはず。
          assert( device == nullptr ); 
          // それぞれに対してアダプタを検索かけたほうが多分いいと思うんだよね。
          _com_ptr_t<_com_IIID<IDXGIAdapter,&__uuidof( IDXGIAdapter )> > pAdapter ([](IDXGIFactory1* dxgi){
              assert( dxgi != nullptr );
              HRESULT hr;
              UINT adapterIndex = 0;
              IDXGIAdapter *adapter(nullptr);
              do{
                hr = dxgi->EnumAdapters( adapterIndex , &adapter );
                {
                  std::wstringstream buffer;
                  buffer << L"EnumAdapters(" << adapterIndex << ")" <<
                    ( (hr == S_OK) ? L"S_OK" : L"error" )<< std::endl;
                  OutputDebugString( buffer.str().c_str() );
                }
                if( hr == S_OK ){ break; }
                assert( adapter == nullptr );
              }while( hr == S_OK && ++adapterIndex);
              return adapter;
            }( dxgifactory ), false  );
          HRESULT hr = D3D10CreateDevice1( pAdapter , driverType , NULL , flags ,levelAttempt,
                                           D3D10_1_SDK_VERSION, &device );
          if( S_OK == hr ){
            assert( device != nullptr );
            // 最初はここで break していたんだけどそれだと、一つ目のforからしか抜けなくてデバイスを二回取っていたので
            // ポインタのリークを起こしていた。正しくは二つのfor文を抜けるために goto で脱出させる。
            goto ENDOFFINDDEVICE; 
          }else{
            assert( device == nullptr );
          }
        }
      }
    ENDOFFINDDEVICE:
      return device;
    }( pDXGIFactory.GetInterfacePtr() ,
       D3D10_CREATE_DEVICE_BGRA_SUPPORT), false);
  if( pD3D10Device1.GetInterfacePtr() == nullptr ){ return 1; }

  {
    std::wstringstream buffer;
    //std::locale defaultLocale("");
    buffer.imbue( std::locale("C") );
    buffer << L"D3DdevicePtr = " << L"0x" <<  pD3D10Device1.GetInterfacePtr() << std::endl;
    OutputDebugString(buffer.str().c_str());
    //buffer.imbue( defaultLocale );
  }

  // ここまでくると DXGIFacotry と、 D3DDevice の二つが手に入るので。
  auto createSwapChain = [](IDXGIFactory1* dxgiFactory1 ,ID3D10Device1 *pD3D10Device1,HWND wnd ){
    IDXGISwapChain *swapChain( nullptr );

    D2D1_SIZE_U size = [](HWND hWnd){ // クライアントウィンドウサイズを取得する
      RECT rc = {0};
      GetClientRect( hWnd, &rc );
      return D2D1::SizeU( rc.right- rc.left, rc.bottom-rc.top );
    }(wnd);

    DXGI_SWAP_CHAIN_DESC swapDesc = {0};
    swapDesc.BufferDesc.Width = size.width;
    swapDesc.BufferDesc.Height = size.height;

    /* // リフレッシュレートの設定 全画面モードで使用するときに使われる。
    swapDesc.BufferDesc.RefreshRate.Numerator = 60; 
    swapDesc.BufferDesc.RefreshRate.Denominator = 1; 
    */
    // リフレッシュレートを自動で設定する場合は、０を指定する。 
    swapDesc.BufferDesc.RefreshRate.Numerator = 60; 
    swapDesc.BufferDesc.RefreshRate.Denominator = 1; 

    //swapDesc.BufferDesc.RefreshRate.Numerator = 0;
    //swapDesc.BufferDesc.RefreshRate.Denominator = 0;

    // バッファのフォーマット
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    // スケーリングファクタ
    swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    // マルチサンプリングパラメータ
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;

    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 3;
    swapDesc.OutputWindow = wnd;
    swapDesc.Windowed = windowMode;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    // DXGI_SWAP_EFFECT_DISCARD;  
    /*
      DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH を設定すると、
      全画面モードのときにサブスクリーンの解像度も変更される。
     */
    // これをすると、GetFrameStatistics がエラーを起こすようになる。なんでだ。
    // swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; 

    dxgiFactory1->CreateSwapChain( pD3D10Device1 , &swapDesc , &swapChain );
    OutputDebugString( windowMode ? _T("WindowMode") : _T("FullScreenMode") );
    return swapChain;
  };

  // スワップチェーンを生成する。
  _com_ptr_t<_com_IIID< IDXGISwapChain,&__uuidof(IDXGISwapChain)> > swapChain( createSwapChain(pDXGIFactory.GetInterfacePtr(),
                                                                                               pD3D10Device1.GetInterfacePtr(),hWnd ), 
                                                                               false );

  if( swapChain.GetInterfacePtr() == nullptr ){ return 1; }

  // IDXGISwapChain のバッファを取得して、 IDXGISurface を生成する
  // TODO: スワップチェインの ResizeBuffers を呼び出したら、これの作り直しもしないとだめ。
#ifdef NOUSE_STATIC_CREATERENDERTARGET_FUNCTION
  _com_ptr_t<_com_IIID<ID2D1RenderTarget, &__uuidof(ID2D1RenderTarget) > > 
    surfaceRenderTarget ([](ID2D1Factory* d2dfactory, IDXGISwapChain* swapChain)  ->ID2D1RenderTarget*  {
        assert( d2dfactory != nullptr );
        assert( swapChain != nullptr );
        
        // DXGI のサーフェースを取得する。
        auto getSurface = [](IDXGISwapChain* swapChain){
          IDXGISurface *backBuffer(nullptr);
          HRESULT hr = swapChain->GetBuffer( 0 , IID_PPV_ARGS( &backBuffer ) );
          return (hr == S_OK) ? backBuffer : nullptr;
        };
        
        _com_ptr_t<_com_IIID< IDXGISurface,&__uuidof( IDXGISurface )> > dxgiSurface( getSurface(swapChain),false);
        if( dxgiSurface.GetInterfacePtr() == nullptr ){
          return nullptr;
        }
        
        D2D1_RENDER_TARGET_PROPERTIES props = [](ID2D1Factory* d2dfactory){
          FLOAT dpiX(0.0f);
          FLOAT dpiY(0.0f);
          d2dfactory->GetDesktopDpi( & dpiX , &dpiY );
          return D2D1::RenderTargetProperties( D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                               D2D1::PixelFormat( DXGI_FORMAT_UNKNOWN , D2D1_ALPHA_MODE_PREMULTIPLIED),
                                               dpiX, dpiY );
        }(d2dfactory);
        
        ID2D1RenderTarget *renderTarget(nullptr);
        HRESULT hr = d2dfactory->CreateDxgiSurfaceRenderTarget(dxgiSurface.GetInterfacePtr() ,props,&renderTarget );
        return ( hr == S_OK ) ? renderTarget : nullptr;
      }(pD2DFactory.GetInterfacePtr(), swapChain ), false );
#else /* NOUSE_STATIC_CREATERENDERTARGET_FUNCTION */
  _com_ptr_t<_com_IIID<ID2D1RenderTarget, &__uuidof(ID2D1RenderTarget) > > surfaceRenderTarget;
  surfaceRenderTarget.Attach( createRenderTarget( swapChain , pD2DFactory ), false);
#endif /* NOUSE_STATIC_CREATERENDERTARGET_FUNCTION */
  if( surfaceRenderTarget.GetInterfacePtr() == nullptr ){
    OutputDebugString(_T(" SurfaceRenderTarget is nullptr "));
    return 1;
  }

  
  OutputDebugString( _T("begin rendering proc"));

  // フレーム毎に呼び出されるラムダ式
  auto proc = [](HWND hWnd ,ID2D1Factory* d2dfactory,IDXGISwapChain *swapChain,ID2D1RenderTarget *target)->int{
    UNREFERENCED_PARAMETER( hWnd );
    UNREFERENCED_PARAMETER( d2dfactory );
    // ダブルバッファリングは、Dwm がしてくれるから。最初に Dwm のコンポジションが終わるの待ってから描画操作を始める
    // Dwm のコンポジションは、DWM が無効なときには、まってくれないので、dxgiOutput のタイミングを使う

      _com_ptr_t<_com_IIID<IDXGIOutput,&__uuidof( IDXGIOutput ) > > dxgiOutput([](IDXGISwapChain *swapChain){
      IDXGIOutput* pOutput(nullptr);
      HRESULT hr = swapChain->GetContainingOutput( &pOutput );
      return (hr == S_OK)? pOutput : nullptr;
      }(swapChain),false);
      dxgiOutput->WaitForVBlank();

    //DwmFlush();
    //Sleep( 5 );
    {
      /*
        VERIFY( S_OK == pD2DFactory->CreateHwndRenderTarget( D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties( hWnd, size ) ,
        &renderTarget ) );
      */
        
      // ブラシの取得
      _com_ptr_t<_com_IIID<ID2D1SolidColorBrush,&_uuidof( ID2D1SolidColorBrush ) > > brush ( [](ID2D1RenderTarget* target){
          ID2D1SolidColorBrush* retval(nullptr);
          return ( S_OK == target->CreateSolidColorBrush( D2D1::ColorF( D2D1::ColorF::Blue) , &retval ) ) ? 
            retval : nullptr ;
        }(target), false );
        
      assert(brush.GetInterfacePtr() != nullptr);
        
      {
        target->BeginDraw();
        target->SetTransform( D2D1::Matrix3x2F::Identity());
        target->Clear( D2D1::ColorF( D2D1::ColorF::White) );
          
        {
          {
            static int count = 0;
            D2D1_ELLIPSE elipse = {0};
            elipse.point.x = cosf( static_cast<FLOAT>( count )/30.0f * ((float)M_PI) *1.0f)*50.f + 100.f;
            elipse.point.y = sinf( static_cast<FLOAT>( count )/30.0f * ((float)M_PI) *1.0f)*50.f + 100.f;
            elipse.radiusX = static_cast<FLOAT>( 10.0f );
            elipse.radiusY = static_cast<FLOAT>( 10.0f );
            target->FillEllipse( &elipse, brush );
            ++count;
            if( count > 120 ){
              count = 0;
            }
          }

          {
            static unsigned int count = 0;
            D2D1_SIZE_U size = target->GetPixelSize();
            D2D1_RECT_F rect = D2D1::RectF( (float)count,0.0f, (float)count + 100, (float)size.height );
            ++count;
            if( count  > size.width ){
              count = 0;
            }
            target->FillRectangle( &rect , brush );
          }

          // カーソルの場所 
          static POINT cursorPos={0};

          VERIFY( GetCursorPos( &cursorPos ) );
          VERIFY( ScreenToClient( hWnd, &cursorPos ) );

            
          for( int j = 0 ; j < 10 ; ++j){
            for( int i = 0 ; i < 10 ; ++i){
              D2D1_ELLIPSE elipse = {0};
              elipse.point.x = static_cast<FLOAT>( cursorPos.x ) + (15.0f * i);
              elipse.point.y = static_cast<FLOAT>( cursorPos.y ) + (15.0f * j);
              elipse.radiusX = 5.0f;
              elipse.radiusY = 5.0f;
              if( brush != nullptr ){
                target->FillEllipse( &elipse , brush );
              }
            }
          }
        }
        VERIFY( S_OK == target->EndDraw() );
      }
        
    } // _com_ptr_t による  renderTarget の開放
    swapChain->Present( 0 , 0 );
    // DWM 
    //ValidateRect( hWnd , NULL );
    return 0;
  };

  retval = [&](){
    for(;;){
      HANDLE handles[] = { continueHandle };
      DWORD value = MsgWaitForMultipleObjects(1 , &(handles[0]) , FALSE , 0 , QS_ALLEVENTS );
      switch(value){
      case WAIT_OBJECT_0:
        //swapChain->Present(1, DXGI_PRESENT_DO_NOT_SEQUENCE);
        proc( hWnd ,  pD2DFactory.GetInterfacePtr(), swapChain.GetInterfacePtr() ,surfaceRenderTarget.GetInterfacePtr() );
        break;
      case ( WAIT_OBJECT_0 + 1 ):
        // これ届いたら PeekMessage で全部処理しないとだめだよね。
        for( ;; ){
          MSG msg = {0};
          BOOL value = PeekMessage( &msg, NULL , 0, 0, PM_REMOVE);
          if( value == 0 ){ goto BREAK_MESSAGE; } // メッセージを受信しなかったときは、ループから抜ける
          if( msg.message == WM_SIZE ){
            unsigned short int width = (msg.lParam & 0xffff);
            unsigned short int height = ((msg.lParam >> 16 )& 0xffff);
            {
              std::wstringstream buffer;
              buffer << L"renderThread recived, WM_SIZE width=" << width << ",height=" << height << std::endl;
              OutputDebugString(buffer.str().c_str());
            }
            // この処理を行うのにメインスレッド側から処理を割り込まれてもらっては困る
            // スワップチェインのバッファサイズの変更と、 レンダリングターゲットのサイズを作り直す。
            {

              // レンダリングターゲットがスワップチェインを保持したまま、リサイズを行うと、
              // （レンダリングターゲットとスワップチェインの解像度が一致しなくなるので) 
              // BitBlt モードでレンダリングするようになる。
              // これはまずいので、先に レンダリングターゲットを開放させます。

              surfaceRenderTarget = nullptr; 
              {
                BOOL screenState(FALSE);
                IDXGIOutput *output(nullptr);
                if( S_OK == swapChain->GetFullscreenState( &screenState , &output ) ){
                  if( screenState ){ // フルスクリーンですよ。
                    
                  }
                }
                if( output != nullptr ){
                  output->Release();
                }
              }
              swapChain->ResizeBuffers( 2 , width , height, DXGI_FORMAT_B8G8R8A8_UNORM, 0);// DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH );
              surfaceRenderTarget.Attach( createRenderTarget( swapChain , pD2DFactory ), false);
            }
          }else if( msg.message == WM_APP_SWITCH_SCREENMODE ){
            windowMode = !(windowMode );
            return 0;
          }
        }
      BREAK_MESSAGE:
        break;
      case WAIT_TIMEOUT:
      default:
        return 1;
      }
    }
  }();
  
  // もし全画面表示だったら、SwapChain を ウィンドウモードに切り替えてから、終了する必要がある。
  [](IDXGISwapChain *swapChain){
    if( swapChain != nullptr ){
      BOOL fullScreen = FALSE;
      if( S_OK == swapChain->GetFullscreenState( &fullScreen , nullptr ) ){
        if( fullScreen ){
          VERIFY( S_OK == swapChain->SetFullscreenState( FALSE , nullptr ) );
        }
      }
    }
  }(swapChain.GetInterfacePtr());

  return retval;
}

