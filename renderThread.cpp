#define _USE_MATH_DEFINES // for M_PI 
#define NOMINMAX // Windows.h �� min max ���}�N����`���Ă���̂ŁA math.h �� min/max �Ƌ�������̂��E��
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
  swapChain �� �o�b�N�o�b�t�@ ����AID2D1Factory::CreateDxgiSurfaceRenderTarget ���g�p���āA
  ID2D1RenderTarget �𐶐�����B
  �����Ɏ��s�����ꍇ�́Anullptr ���߂�B
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

  ID2D1RenderTarget *renderTarget(nullptr); // �߂�l
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
  DXGI�̃X�N���[�����[�h�؂�ւ��́A���C���X���b�h�̃��b�Z�[�W�t�b�N�ōs����̂ŁA�؂�ւ����̂����C���X���b�h�ł�����B
  ���ɂȂ�̂́AIDXGISurface �̓����̖��ł���B

  ���C���X���b�h����؂�ւ����s����ƁA IDXGISwapChain �̃o�b�N�o�b�t�@�̍Đݒ肪�s���A
  ���̃X���b�h�ŕێ����Ă��� IDXGISurface �������ɂȂ�BIDXGISurface �������ɂȂ�Ƃ������Ƃ�
  �����_�����O�^�[�Q�b�g�������ɂȂ�B
  �����_�����O�X���b�h�� �E�B���h�E�̃��b�Z�[�W�|���v�X���b�h���������Ă���ꍇ�A
  ���̂悤�ȗ��R�ɂ��MakeWindowAssociation �� alt+enter �̑S��ʕ\���ւ̐؂�ւ��͋֎~����B
*/

unsigned int renderthreadEntryPoint(HWND hWnd,HANDLE continueHandle){
  // �E�B���h�E���[�h
  static BOOL windowMode = TRUE;

  unsigned int retval(0);
  assert( hWnd != NULL );
  assert( continueHandle != NULL );

  if( continueHandle == NULL ){
    return 1;
  }
  // �X���b�h���b�Z�[�W�L���[�𐶐����āA continueHandle �̃C�x���g�𗧂ĂĂ���
  [](HANDLE continueHandle){ 
    // PeekMessage ����ƃX���b�h���b�Z�[�W�L���[�����������B
    MSG msg = {0};
    PeekMessage( &msg, NULL , WM_USER , WM_USER , PM_NOREMOVE ); 
    // ���b�Z�[�W�L���[���������ꂽ�̂ŁA�C�x���g�𗧂��グ��B
    SetEvent(continueHandle);
  }(continueHandle);


  // �f�o�C�X��ˑ������̏�����

  // ID2D1Factory �̃C���^�[�t�F�[�X�|�C���^�i�̃X�}�[�g�|�C���^�j
  _com_ptr_t<_com_IIID<ID2D1Factory, &__uuidof(ID2D1Factory)> > pD2DFactory([](){
      ID2D1Factory *factory(nullptr);
      HRESULT hr;
      hr = D2D1CreateFactory( D2D1_FACTORY_TYPE_MULTI_THREADED , &factory );
      return (hr==S_OK) ? factory : nullptr ;
    }(), false );
  if( pD2DFactory.GetInterfacePtr() == nullptr ){  return 1;  }

  // DXGIFactory1 �̃C���^�[�t�F�[�X�|�C���^
  _com_ptr_t<_com_IIID<IDXGIFactory1, &__uuidof(IDXGIFactory1)> > pDXGIFactory([](){
      IDXGIFactory1 *ptr(nullptr);
      HRESULT hr = CreateDXGIFactory1( __uuidof( IDXGIFactory1 ), reinterpret_cast<void**>(&ptr) );
      return ( hr == S_OK )? ptr : nullptr;
    }(), false );
  if( pDXGIFactory.GetInterfacePtr() == nullptr ){  return 1;  }
  // �ʂ̃X���b�h���璼�� DXGI �̏�Ԃ�ς�����Ɣ��ɂ܂����̂ŁA����́A�֎~�ɂ��Ă���
  pDXGIFactory->MakeWindowAssociation( NULL , DXGI_MWA_NO_WINDOW_CHANGES ); 
  
  // D3D10Device1 �̐���
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
          // ���� goto ���̃R�����g�ɑΉ����� �h�q�I assert �� 
          // �����ɓ��B����Ƃ��� device �� nullptr �ł���͂��B
          assert( device == nullptr ); 
          // ���ꂼ��ɑ΂��ăA�_�v�^�������������ق������������Ǝv���񂾂�ˁB
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
            // �ŏ��͂����� break ���Ă����񂾂��ǂ��ꂾ�ƁA��ڂ�for���炵�������Ȃ��ăf�o�C�X�������Ă����̂�
            // �|�C���^�̃��[�N���N�����Ă����B�������͓��for���𔲂��邽�߂� goto �ŒE�o������B
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

  // �����܂ł���� DXGIFacotry �ƁA D3DDevice �̓����ɓ���̂ŁB
  auto createSwapChain = [](IDXGIFactory1* dxgiFactory1 ,ID3D10Device1 *pD3D10Device1,HWND wnd ){
    IDXGISwapChain *swapChain( nullptr );

    D2D1_SIZE_U size = [](HWND hWnd){ // �N���C�A���g�E�B���h�E�T�C�Y���擾����
      RECT rc = {0};
      GetClientRect( hWnd, &rc );
      return D2D1::SizeU( rc.right- rc.left, rc.bottom-rc.top );
    }(wnd);

    DXGI_SWAP_CHAIN_DESC swapDesc = {0};
    swapDesc.BufferDesc.Width = size.width;
    swapDesc.BufferDesc.Height = size.height;

    /* // ���t���b�V�����[�g�̐ݒ� �S��ʃ��[�h�Ŏg�p����Ƃ��Ɏg����B
    swapDesc.BufferDesc.RefreshRate.Numerator = 60; 
    swapDesc.BufferDesc.RefreshRate.Denominator = 1; 
    */
    // ���t���b�V�����[�g�������Őݒ肷��ꍇ�́A�O���w�肷��B 
    swapDesc.BufferDesc.RefreshRate.Numerator = 60; 
    swapDesc.BufferDesc.RefreshRate.Denominator = 1; 

    //swapDesc.BufferDesc.RefreshRate.Numerator = 0;
    //swapDesc.BufferDesc.RefreshRate.Denominator = 0;

    // �o�b�t�@�̃t�H�[�}�b�g
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    // �X�P�[�����O�t�@�N�^
    swapDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    // �}���`�T���v�����O�p�����[�^
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;

    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 3;
    swapDesc.OutputWindow = wnd;
    swapDesc.Windowed = windowMode;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    // DXGI_SWAP_EFFECT_DISCARD;  
    /*
      DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH ��ݒ肷��ƁA
      �S��ʃ��[�h�̂Ƃ��ɃT�u�X�N���[���̉𑜓x���ύX�����B
     */
    // ���������ƁAGetFrameStatistics ���G���[���N�����悤�ɂȂ�B�Ȃ�ł��B
    // swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; 

    dxgiFactory1->CreateSwapChain( pD3D10Device1 , &swapDesc , &swapChain );
    OutputDebugString( windowMode ? _T("WindowMode") : _T("FullScreenMode") );
    return swapChain;
  };

  // �X���b�v�`�F�[���𐶐�����B
  _com_ptr_t<_com_IIID< IDXGISwapChain,&__uuidof(IDXGISwapChain)> > swapChain( createSwapChain(pDXGIFactory.GetInterfacePtr(),
                                                                                               pD3D10Device1.GetInterfacePtr(),hWnd ), 
                                                                               false );

  if( swapChain.GetInterfacePtr() == nullptr ){ return 1; }

  // IDXGISwapChain �̃o�b�t�@���擾���āA IDXGISurface �𐶐�����
  // TODO: �X���b�v�`�F�C���� ResizeBuffers ���Ăяo������A����̍�蒼�������Ȃ��Ƃ��߁B
#ifdef NOUSE_STATIC_CREATERENDERTARGET_FUNCTION
  _com_ptr_t<_com_IIID<ID2D1RenderTarget, &__uuidof(ID2D1RenderTarget) > > 
    surfaceRenderTarget ([](ID2D1Factory* d2dfactory, IDXGISwapChain* swapChain)  ->ID2D1RenderTarget*  {
        assert( d2dfactory != nullptr );
        assert( swapChain != nullptr );
        
        // DXGI �̃T�[�t�F�[�X���擾����B
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

  // �t���[�����ɌĂяo����郉���_��
  auto proc = [](HWND hWnd ,ID2D1Factory* d2dfactory,IDXGISwapChain *swapChain,ID2D1RenderTarget *target)->int{
    UNREFERENCED_PARAMETER( hWnd );
    UNREFERENCED_PARAMETER( d2dfactory );
    // �_�u���o�b�t�@�����O�́ADwm �����Ă���邩��B�ŏ��� Dwm �̃R���|�W�V�������I���̑҂��Ă���`�摀����n�߂�
    // Dwm �̃R���|�W�V�����́ADWM �������ȂƂ��ɂ́A�܂��Ă���Ȃ��̂ŁAdxgiOutput �̃^�C�~���O���g��

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
        
      // �u���V�̎擾
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

          // �J�[�\���̏ꏊ 
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
        
    } // _com_ptr_t �ɂ��  renderTarget �̊J��
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
        // ����͂����� PeekMessage �őS���������Ȃ��Ƃ��߂���ˁB
        for( ;; ){
          MSG msg = {0};
          BOOL value = PeekMessage( &msg, NULL , 0, 0, PM_REMOVE);
          if( value == 0 ){ goto BREAK_MESSAGE; } // ���b�Z�[�W����M���Ȃ������Ƃ��́A���[�v���甲����
          if( msg.message == WM_SIZE ){
            unsigned short int width = (msg.lParam & 0xffff);
            unsigned short int height = ((msg.lParam >> 16 )& 0xffff);
            {
              std::wstringstream buffer;
              buffer << L"renderThread recived, WM_SIZE width=" << width << ",height=" << height << std::endl;
              OutputDebugString(buffer.str().c_str());
            }
            // ���̏������s���̂Ƀ��C���X���b�h�����珈�������荞�܂�Ă�����Ă͍���
            // �X���b�v�`�F�C���̃o�b�t�@�T�C�Y�̕ύX�ƁA �����_�����O�^�[�Q�b�g�̃T�C�Y����蒼���B
            {

              // �����_�����O�^�[�Q�b�g���X���b�v�`�F�C����ێ������܂܁A���T�C�Y���s���ƁA
              // �i�����_�����O�^�[�Q�b�g�ƃX���b�v�`�F�C���̉𑜓x����v���Ȃ��Ȃ�̂�) 
              // BitBlt ���[�h�Ń����_�����O����悤�ɂȂ�B
              // ����͂܂����̂ŁA��� �����_�����O�^�[�Q�b�g���J�������܂��B

              surfaceRenderTarget = nullptr; 
              {
                BOOL screenState(FALSE);
                IDXGIOutput *output(nullptr);
                if( S_OK == swapChain->GetFullscreenState( &screenState , &output ) ){
                  if( screenState ){ // �t���X�N���[���ł���B
                    
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
  
  // �����S��ʕ\����������ASwapChain �� �E�B���h�E���[�h�ɐ؂�ւ��Ă���A�I������K�v������B
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

