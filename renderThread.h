
// �����_�����O�X���b�h�ɑ΂��đ��郁�b�Z�[�W
enum{
  WM_APP_SUBTHREAD_BEGIN =  (WM_APP+1),
  WM_APP_SUBTHREAD_TERMINATE ,
  WM_APP_SWITCH_SCREENMODE, 
  WM_APP_SUBTHREAD_END
};

unsigned int renderthreadEntryPoint(HWND hWnd,HANDLE continueHandle);
