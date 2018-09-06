
#include <TCHAR.h>
#include <Windows.h>
#include <cassert>

const TCHAR* buttonAtom=TEXT("D2D1UIButton");

struct D2D1UIButtonWndClass{
  ATOM wndClassAtom;
  D2D1UIButtonWndClass();
  ~D2D1UIButtonWndClass();
  static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam );
};

D2D1UIButtonWndClass btnClass;

LRESULT CALLBACK D2D1UIButtonWndClass::wndProc(HWND hwnd, UINT msg, WPARAM wParam , LPARAM lParam ){
  return ::DefWindowProc( hwnd, msg, wParam , lParam );
}

D2D1UIButtonWndClass::D2D1UIButtonWndClass():wndClassAtom(NULL){
  WNDCLASSEX wndclass = {0};
  wndclass.cbSize = sizeof( WNDCLASSEX );
  wndclass.style = CS_HREDRAW | CS_VREDRAW ;
  wndclass.lpfnWndProc = D2D1UIButtonWndClass::wndProc;
  wndclass.cbClsExtra = 0;
  wndclass.cbWndExtra = 0;
  wndclass.hInstance = GetModuleHandle( NULL );
  wndclass.hIcon = NULL;
  wndclass.hCursor = LoadCursor( NULL , IDC_ARROW);
  wndclass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wndclass.lpszMenuName = NULL;
  wndclass.lpszClassName = buttonAtom;
  wndclass.hIconSm = NULL;

  wndClassAtom = RegisterClassEx( &wndclass );
  assert( wndClassAtom != NULL );

  OutputDebugString( TEXT("D2D1UIButtonWndClass") );
  return;
}

D2D1UIButtonWndClass::~D2D1UIButtonWndClass(){
  BOOL value = UnregisterClass( (LPCTSTR) wndClassAtom , GetModuleHandle( NULL ) );
  OutputDebugString( TEXT("~D2D1UIButtonWndClass" ));
  assert( value );
  return;
}
