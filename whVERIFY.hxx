/**
   WindowsHelperLibrary: whVERFIY.h
   �x���t�@�C�p�}�N���w�b�_�t�@�C��
 */
#ifndef WH_VERIFY_HXX_HEADER_GUARD
#define WH_VERIFY_HXX_HEADER_GUARD 1

#ifdef __cplusplus
#include <cassert>
#else  /* __cplusplus */
#include <assert.h>
#endif /*  __cplusplus */

/**************************
   MFC VERIFY macro like  define  
 **************************/
#ifndef VERIFY

#ifdef NDEBUG 
#define VERIFY(expression) (expression)
#else /* NDEBUG */
#define VERIFY(expression) assert( expression )
#endif /* NDEBUG */

#endif  /* VERIFY*/

/* 
   BOOLEVAL( exp ) 
   WinAPI �� BOOL �l��Ԃ�API ���Ăяo���Ƃ��̕]���p�̃}�N�� 
   WinAPI �� BOOL��Ԃ� API �̕]���̎d���́A if( Method(...) ){ }else{ } �� �]�����邪�A
   ( Windows�ɂ�����R�[�f�B���O�K�� : http://msdn.microsoft.com/ja-jp/library/ff381404%28v=vs.85%29)
   �߂�l�̃`�F�b�N�𗝉����Â炢�̂ŁA���ɒP���ȃ}�N�������܂��Ă���B�߂�l��c++�� true �������� false 

   ���Ӂj ���̃}�N�����g�p���Ă��邩��Ƃ����āAAPI�̐������`�F�b�N�����Ă��邩�ǂ����͎��ۂ̃R�[�f�B���O����ł���B
   �L�q�҂� ���� exp �� BOOL �ł����Ă���Ǝv������ł���ꍇ������B
*/
namespace wh{
  inline bool boolvalue( BOOL eval ){ return (( eval )? true : false ); }
};
#ifndef BOOLVAL
#define BOOLEVAL(exp) (wh::boolvalue(exp))
#endif /* BOOLVAL */
/**
   NOTZERO(exp) 
   WinAPI �� 0 �������� ����ȊO��Ԃ�API ���Ăяo���Ƃ��̕]���p�}�N��

   ���Ӂj ���̃}�N�����g�p���Ă��邩��Ƃ����āAAPI�̐������`�F�b�N�����Ă��邩�ǂ����͎��ۂ̃R�[�f�B���O����ł���B
   �L�q�҂� ���� exp �� 0 or ��[���l �ł����Ă���Ǝv������ł���ꍇ������B
 */
#ifndef NOTZERO
#define NOTZERO( exp ) (( 0 != (exp) )?true: false)
#endif /* NOTZERO */

/*
  assert �̒��ɓ����e�L�X�g���܂߂邽�߂̕������܂ރ}�N�� 
  ���̕��� �����̍Ōオ���̒l�ɂȂ�Ƃ����AC/C++�̎d�l�𗘗p����B
 */
#ifndef ASSERT_TEXT
#define ASSERT_TEXT( exp, comment ) ((comment),(exp))
#endif /* ASSERT_TEXT */

#endif /* WH_VERIFY_HXX_HEADER_GUARD */
