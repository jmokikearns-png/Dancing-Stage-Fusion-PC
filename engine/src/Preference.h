/* Preference - Holds user-chosen preferences that are saved between sessions. */

#ifndef PREFERENCE_H
#define PREFERENCE_H

#include "EnumHelper.h"
class IniFile;

struct lua_State;
class IPreference
{
public:
	IPreference( const CString& sName );
	/* Copies register themselves too, so a copy's destructor can unsubscribe.
	 * (GCC copies objects passed through "..."; never pass a Preference to a
	 * printf-style function -- pass .Get().) */
	IPreference( const IPreference &cpy );
	virtual ~IPreference();

	virtual void LoadDefault() = 0;
	virtual void ReadFrom( const IniFile &ini );
	virtual void WriteTo( IniFile &ini ) const;

	virtual CString ToString() const = 0;
	virtual void FromString( const CString &s ) = 0;

	virtual void SetFromStack( lua_State *L );
	virtual void PushValue( lua_State *L ) const;

	const CString &GetName() const { return m_sName; }

protected:
	CString		m_sName;
};

void BroadcastPreferenceChanged( const CString& sPreferenceName );

template <class T>
class Preference : public IPreference
{
private:
	// Historically currentValue came first so that this object could be passed
	// to printf-style functions on 32-bit MSVC.  That does not work on x64
	// (large objects go through "..." by reference) or with GCC; use Get().
	T			m_currentValue;
	T			m_defaultValue;
	
public:
	Preference( const CString& sName, const T& defaultValue ):
		IPreference( sName ),
		m_currentValue( defaultValue ),
		m_defaultValue( defaultValue )
	{
		LoadDefault();
	}

	CString ToString() const;
	void FromString( const CString &s );
	void SetFromStack( lua_State *L );
	void PushValue( lua_State *L ) const;

	void LoadDefault()
	{
		m_currentValue = m_defaultValue;
	}

	T Get() const
	{
		return m_currentValue;
	}
	
	operator const T () const
	{
		return Get();
	}
	
	void Set( const T& other )
	{
		m_currentValue = other;
		BroadcastPreferenceChanged( m_sName );
	}
};

#endif

/*
 * (c) 2001-2004 Chris Danford, Chris Gomez
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
