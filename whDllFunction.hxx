/**

 */
#ifndef WH_DLLFUNCTION_HXX
#define WH_DLLFUNCTION_HXX

/* Windows 7, Windows Server 2008 R2, Windows Vista, and Windows Server 2008 の KB2533623 適用すると有効になるフラグ */
/* これは SDK version 7.1A には入っていないので、手前で定義をかける */
#ifndef LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
#define LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR (0x00000100)
#endif /* LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR */

#ifndef LOAD_LIBRARY_SEARCH_APPLICATION_DIR
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR (0x00000200)
#endif /* LOAD_LIBRARY_SEARCH_APPLICATION_DIR */

#ifndef LOAD_LIBRARY_SEARCH_USER_DIRS
#define LOAD_LIBRARY_SEARCH_USER_DIRS (0x00000400)
#endif /* LOAD_LIBRARY_SEARCH_USER_DIRS */

#ifndef LOAD_LIBRARY_SEARCH_SYSTEM32
#define LOAD_LIBRARY_SEARCH_SYSTEM32 (0x00000800)
#endif /* LOAD_LIBRARY_SEARCH_SYSTEM32 */

#ifndef LOAD_LIBRARY_SEARCH_DEFAULT_DIRS
#define LOAD_LIBRARY_SEARCH_DEFAULT_DIRS (0x00001000)
#endif /* LOAD_LIBRARY_SEARCH_DEFAULT_DIRS */

namespace wh{

  template <typename functionType,DWORD loadExFlag = LOAD_LIBRARY_SEARCH_SYSTEM32,int useLoadLibrary = 1>
  class ScopedDllFunction{
  public:
    typedef functionType *FunctionPtrType;
    enum{ LOADLIBRARYEX_DWFLAG = loadExFlag }; /* LoadLibraryEx において使う フラグ*/
    enum{ USELOADLIBRARY = useLoadLibrary }; /* LoadLibraryEx/FreeLibrary の組み合わせか GetModuleHandle を使うか */
  private:
    HMODULE module; /* モジュールのハンドル */
    FunctionPtrType function_ptr; /* 関数ポインタ */
  private:
    /* 通常のテンプレートは、LoadLibraryEx でモジュールハンドルを取得して FreeLibrary で開放する。*/
    template<int loadType> inline HMODULE getHModule( const TCHAR * dll_name ){
      return ::LoadLibraryEx( dll_name , NULL , LOADLIBRARYEX_DWFLAG );
    }
    template<int loadType> inline BOOL releaseModule(){
      BOOL retval(TRUE);
      if( NULL != module ){ 
        retval = ::FreeLibrary( module ); 
      }
      module = NULL;
      return retval;
    }
    /* テンプレートの特殊化を行い GetModuleHandle を使うようにする(GetModuleHandle で取得したモジュールハンドルの開放は無い) */
    template<> inline HMODULE getHModule<0>( const TCHAR *dll_name ){  return ::GetModuleHandle( dll_name );  }
    template<> inline BOOL releaseModule<0>(){  module = NULL;  return TRUE;  }
    
  public:
    // これ本当はコンストラクタで例外飛ばすようにしたほうがいいと思うんだよね。
    ScopedDllFunction( const TCHAR *const dll_name , const char* const function_name) :
      module(NULL),function_ptr( nullptr ){

      module = getHModule<USELOADLIBRARY>(dll_name);
      if( NULL != module ){
        function_ptr = reinterpret_cast<FunctionPtrType>( GetProcAddress( module , function_name ) );
      }

      return;
    }
    /** デストラクタ */
    ~ScopedDllFunction(){
      releaseModule<USELOADLIBRARY>();
      return;
    }
    
    /** モジュールのファイル名を取得する */
    inline DWORD GetModuleFileName( LPTSTR lpFilename , DWORD nSize ){
      return (( NULL != module ) ? (::GetModuleFileName( module , lpFilename , nSize )) : 0);
    }

    /** 関数ポインタの取得 */
    inline operator FunctionPtrType () const {  return function_ptr; }

  private:
    /* コピーコンストラクタの禁止 */
    ScopedDllFunction( const ScopedDllFunction &);
    /* コピー禁止 */
    const ScopedDllFunction& operator = ( const ScopedDllFunction &);
  };

  /* サーチパスからカレントワーキングディレクトリを排除する */
  inline BOOL LibraryPathExcludeCurrentWorkingDirectory(){
    ScopedDllFunction< decltype(SetDllDirectoryW) , 0 , 0 > setDllDirectoryW( _T("kernel32.dll"), "SetDllDirectoryW");
    if( (setDllDirectoryW) ){
      if( 0 != setDllDirectoryW(L"") ){ 
        return TRUE;
      }
    }
    return FALSE;
  };

};

#endif /* WH_DLLFUNCTION_HXX */
