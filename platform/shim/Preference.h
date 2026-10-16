/* dsfplatform shim for Preference.h.
 * The real Preference<T> registers itself with PrefsManager, broadcasts changes
 * through MessageManager and exposes itself to Lua.  The platform layer only needs
 * a typed value with the same interface (Get(), implicit conversion, Set()). */
#ifndef PREFERENCE_H
#define PREFERENCE_H

#include "EnumHelper.h"

template <class T>
class Preference
{
	T m_currentValue;
	T m_defaultValue;
	CString m_sName;
public:
	Preference( const CString& sName, const T& defaultValue ):
		m_currentValue( defaultValue ), m_defaultValue( defaultValue ), m_sName( sName ) { }
	void LoadDefault() { m_currentValue = m_defaultValue; }
	T Get() const { return m_currentValue; }
	operator const T () const { return Get(); }
	void Set( const T& other ) { m_currentValue = other; }
	const CString &GetName() const { return m_sName; }
};

#endif
