/**

 */
#ifndef WH_DLLFUNCTION_HXX
#define WH_DLLFUNCTION_HXX

/* Windows 7, Windows Server 2008 R2, Windows Vista, and Windows Server 2008 �� KB2533623 �K�p����ƗL���ɂȂ�t���O */
/* ����� SDK version 7.1A �ɂ͓����Ă��Ȃ��̂ŁA��O�Œ�`�������� */
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
    enum{ LOADLIBRARYEX_DWFLAG = loadExFlag }; /* LoadLibraryEx �ɂ����Ďg�� �t���O*/
    enum{ USELOADLIBRARY = useLoadLibrary }; /* LoadLibraryEx/FreeLibrary �̑g�ݍ��킹�� GetModuleHandle ���g���� */
  private:
    HMODULE module; /* ���W���[���̃n���h�� */
    FunctionPtrType function_ptr; /* �֐��|�C���^ */
  private:
    /* �ʏ�̃e���v���[�g�́ALoadLibraryEx �Ń��W���[���n���h�����擾���� FreeLibrary �ŊJ������B*/
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
    /* �e���v���[�g�̓��ꉻ���s�� GetModuleHandle ���g���悤�ɂ���(GetModuleHandle �Ŏ擾�������W���[���n���h���̊J���͖���) */
    template<> inline HMODULE getHModule<0>( const TCHAR *dll_name ){  return ::GetModuleHandle( dll_name );  }
    template<> inline BOOL releaseModule<0>(){  module = NULL;  return TRUE;  }
    
  public:
    // ����{���̓R���X�g���N�^�ŗ�O��΂��悤�ɂ����ق��������Ǝv���񂾂�ˁB
    ScopedDllFunction( const TCHAR *const dll_name , const char* const function_name) :
      module(NULL),function_ptr( nullptr ){

      module = getHModule<USELOADLIBRARY>(dll_name);
      if( NULL != module ){
        function_ptr = reinterpret_cast<FunctionPtrType>( GetProcAddress( module , function_name ) );
      }

      return;
    }
    /** �f�X�g���N�^ */
    ~ScopedDllFunction(){
      releaseModule<USELOADLIBRARY>();
      return;
    }
    
    /** ���W���[���̃t�@�C�������擾���� */
    inline DWORD GetModuleFileName( LPTSTR lpFilename , DWORD nSize ){
      return (( NULL != module ) ? (::GetModuleFileName( module , lpFilename , nSize )) : 0);
    }

    /** �֐��|�C���^�̎擾 */
    inline operator FunctionPtrType () const {  return function_ptr; }

  private:
    /* �R�s�[�R���X�g���N�^�̋֎~ */
    ScopedDllFunction( const ScopedDllFunction &);
    /* �R�s�[�֎~ */
    const ScopedDllFunction& operator = ( const ScopedDllFunction &);
  };

  /* �T�[�`�p�X����J�����g���[�L���O�f�B���N�g����r������ */
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
