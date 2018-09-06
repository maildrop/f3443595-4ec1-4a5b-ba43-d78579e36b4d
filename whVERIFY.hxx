/**
   WindowsHelperLibrary: whVERFIY.h
   ベリファイ用マクロヘッダファイル
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
   WinAPI で BOOL 値を返すAPI を呼び出すときの評価用のマクロ 
   WinAPI の BOOLを返す API の評価の仕方は、 if( Method(...) ){ }else{ } で 評価するが、
   ( Windowsにおけるコーディング規約 : http://msdn.microsoft.com/ja-jp/library/ff381404%28v=vs.85%29)
   戻り値のチェックを理解しづらいので、非常に単純なマクロを噛ませている。戻り値はc++の true もしくは false 

   注意） このマクロを使用しているからといって、APIの正しいチェックをしているかどうかは実際のコーディング次第である。
   記述者が この exp が BOOL であってあると思い込んでいる場合もある。
*/
namespace wh{
  inline bool boolvalue( BOOL eval ){ return (( eval )? true : false ); }
};
#ifndef BOOLVAL
#define BOOLEVAL(exp) (wh::boolvalue(exp))
#endif /* BOOLVAL */
/**
   NOTZERO(exp) 
   WinAPI で 0 もしくは それ以外を返すAPI を呼び出すときの評価用マクロ

   注意） このマクロを使用しているからといって、APIの正しいチェックをしているかどうかは実際のコーディング次第である。
   記述者が この exp が 0 or 非ゼロ値 であってあると思い込んでいる場合もある。
 */
#ifndef NOTZERO
#define NOTZERO( exp ) (( 0 != (exp) )?true: false)
#endif /* NOTZERO */

/*
  assert の中に入れるテキストを含めるための複文を含むマクロ 
  この文は 複文の最後が文の値になるという、C/C++の仕様を利用する。
 */
#ifndef ASSERT_TEXT
#define ASSERT_TEXT( exp, comment ) ((comment),(exp))
#endif /* ASSERT_TEXT */

#endif /* WH_VERIFY_HXX_HEADER_GUARD */
