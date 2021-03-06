/*
 * Copyright (C) 2004-2009  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef _UTILS_H
#define _UTILS_H

#include <assert.h>
#include <cstdio>
#include <fcntl.h>
#include <map>
#include <sys/file.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <iostream>

using std::map;
using std::vector;
using std::pair;
using std::cout;
using std::endl;

typedef void (*outputHook)(int type, const char* text, void *userData);

#define DEBUG(f) do { \
	if (CUtils::Debug()) { \
		std::stringstream sTmpDbg; \
		sTmpDbg << f << endl; \
		CUtils::PrintDebug(sTmpDbg.str()); \
	} \
} while (0);

static inline void SetFdCloseOnExec(int fd)
{
#ifndef _WIN32
	int flags = fcntl(fd, F_GETFD, 0);
	if (flags < 0)
		return; // Ignore errors
	// When we execve() a new process this fd is now automatically closed.
	fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
#endif // ! _WIN32
}

static const char g_HexDigits[] = "0123456789abcdef";

class ZNC_API CUtils {
public:
	CUtils();
	~CUtils();

	static CString GetIP(unsigned long addr);
	static unsigned long GetLongIP(const CString& sIP);
	static void SetStdoutIsTTY(bool b) { stdoutIsTTY = b; }
	static bool StdoutIsTTY() { return stdoutIsTTY; }
	static void SetDebug(bool b) { debug = b; }
	static bool Debug() { return debug; }
	static void HookOutput(outputHook fHook, void* userData) { outputHook = fHook; outputHookUserData = userData; }
	static bool OutputHooked() { return (outputHook != NULL); }
	static void HookedOutput(int type, const CString& message) { if(OutputHooked()) { outputHook(type, message.c_str(), outputHookUserData); } }

	static void PrintError(const CString& sMessage);
	static void PrintDebug(const CString& sMessage);
	static void PrintMessage(const CString& sMessage, bool bStrong = false);
	static void PrintPrompt(const CString& sMessage);
	static void PrintAction(const CString& sMessage);
	static void PrintStatus(bool bSuccess, const CString& sMessage = "");

	static const CString sDefaultHash;

	static CString GetSaltedHashPass(CString& sSalt);
	static CString GetSalt();
	static CString SaltedMD5Hash(const CString& sPass, const CString& sSalt);
	static CString SaltedSHA256Hash(const CString& sPass, const CString& sSalt);
	static CString GetPass(const CString& sPrompt);
	static bool GetInput(const CString& sPrompt, CString& sRet, const CString& sDefault = "", const CString& sHint = "");
	static bool GetBoolInput(const CString& sPrompt, bool bDefault);
	static bool GetBoolInput(const CString& sPrompt, bool *pbDefault = NULL);
	static bool GetNumInput(const CString& sPrompt, unsigned int& uRet, unsigned int uMin = 0, unsigned int uMax = ~0, unsigned int uDefault = ~0);

	static unsigned long long GetMillTime() {
		struct timeval tv;
		unsigned long long iTime = 0;
		gettimeofday(&tv, NULL);
		iTime = (unsigned long long) tv.tv_sec * 1000;
		iTime += ((unsigned long long) tv.tv_usec / 1000);
		return iTime;
	}
#ifdef HAVE_LIBSSL
	static void GenerateCert(FILE *pOut, const CString& sHost = "");
#endif /* HAVE_LIBSSL */

private:
protected:
	static bool stdoutIsTTY;
	static bool debug;
	static outputHook outputHook;
	static void* outputHookUserData;
};

class CException {
public:
	typedef enum {
		EX_Shutdown,
		EX_Restart
	} EType;

	CException(EType e) {
		m_eType = e;
	}
	virtual ~CException() {}

	EType GetType() const { return m_eType; }
private:
protected:
	EType	m_eType;
};


class ZNC_API CTable : protected vector<vector<CString> > {
public:
	CTable() {}
	virtual ~CTable() {}

	bool AddColumn(const CString& sName);
	size_t AddRow();
	bool SetCell(const CString& sColumn, const CString& sValue, size_t uRowIdx = ~0);
	bool GetLine(size_t uIdx, CString& sLine) const;

	size_t GetColumnWidth(size_t uIdx) const;

	void Clear();
	using vector<vector<CString> >::size;
private:
	unsigned int GetColumnIndex(const CString& sName) const;

protected:
	vector<CString>				m_vsHeaders;
	map<CString, size_t>		m_msuWidths;	// Used to cache the width of a column
};


#ifdef HAVE_LIBSSL
#include <openssl/blowfish.h>
#include <openssl/md5.h>
//! does Blowfish w/64 bit feedback, no padding
class ZNC_API CBlowfish {
public:
	/**
	 * @param sPassword key to encrypt with
	 * @param iEncrypt encrypt method (BF_DECRYPT or BF_ENCRYPT)
	 * @param sIvec what to set the ivector to start with, default sets it all 0's
	 */
	CBlowfish(const CString & sPassword, int iEncrypt, const CString & sIvec = "");
	~CBlowfish();

	//! output must be freed
	static unsigned char *MD5(const unsigned char *input, size_t ilen);

	//! returns an md5 of the CString (not hex encoded)
	static CString MD5(const CString & sInput, bool bHexEncode = false);

	//! output must be the same size as input
	void Crypt(unsigned char *input, unsigned char *output, size_t ibytes);

	//! must free result
	unsigned char * Crypt(unsigned char *input, size_t ibytes);
	CString Crypt(const CString & sData);

private:
	unsigned char		*m_ivec;
	BF_KEY 				m_bkey;
	int					m_iEncrypt, m_num;
};

#endif /* HAVE_LIBSSL */

/**
 * @class TCacheMap
 * @author prozac <prozac@rottenboy.com>
 * @brief Insert an object with a time-to-live and check later if it still exists
 */
template<typename K, typename V = bool>
class TCacheMap {
public:
	TCacheMap(unsigned int uTTL = 5000) {
		m_uTTL = uTTL;
	}

	virtual ~TCacheMap() {}

	/**
	 * @brief This function adds an item to the cache using the default time-to-live value
	 * @param Item the item to add to the cache
	 */
	void AddItem(const K& Item) {
		AddItem(Item, m_uTTL);
	}

	/**
	 * @brief This function adds an item to the cache using a custom time-to-live value
	 * @param Item the item to add to the cache
	 * @param uTTL the time-to-live for this specific item
	 */
	void AddItem(const K& Item, unsigned int uTTL) {
		if (!uTTL) {			// If time-to-live is zero we don't want to waste our time adding it
			RemItem(Item);		// Remove the item incase it already exists
			return;
		}

		m_mItems[Item] = value(CUtils::GetMillTime() + uTTL, V());
	}

	/**
	 * @brief This function adds an item to the cache using the default time-to-live value
	 * @param Item the item to add to the cache
	 * @param Val The value associated with the key Item
	 */
	void AddItem(const K& Item, const V& Val) {
		AddItem(Item, Val, m_uTTL);
	}

	/**
	 * @brief This function adds an item to the cache using a custom time-to-live value
	 * @param Item the item to add to the cache
	 * @param Val The value associated with the key Item
	 * @param uTTL the time-to-live for this specific item
	 */
	void AddItem(const K& Item, const V& Val, unsigned int uTTL) {
		if (!uTTL) {			// If time-to-live is zero we don't want to waste our time adding it
			RemItem(Item);		// Remove the item incase it already exists
			return;
		}

		m_mItems[Item] = value(CUtils::GetMillTime() + uTTL, Val);
	}

	/**
	 * @brief Performs a Cleanup() and then checks to see if your item exists
	 * @param Item The item to check for
	 * @return true if item exists
	 */
	bool HasItem(const K& Item) {
		Cleanup();
		return (m_mItems.find(Item) != m_mItems.end());
	}

	/**
	 * @brief Performs a Cleanup() and returns a pointer to the object, or NULL
	 * @param Item The item to check for
	 * @return Pointer to the item or NULL if there is no suitable one
	 */
	V* GetItem(const K& Item) {
		Cleanup();
		iterator it = m_mItems.find(Item);
		if (it == m_mItems.end())
			return NULL;
		return &it->second.second;
	}

	/**
	 * @brief Removes a specific item from the cache
	 * @param Item The item to be removed
	 * @return true if item existed and was removed, false if it never existed
	 */
	bool RemItem(const K& Item) {
		return (m_mItems.erase(Item) != 0);
	}

	/**
	 * @brief Cycles through the queue removing all of the stale entries
	 */
	void Cleanup() {
		iterator it = m_mItems.begin();

		while (it != m_mItems.end()) {
			if (CUtils::GetMillTime() > (it->second.first)) {
				m_mItems.erase(it++);
			} else {
				++it;
			}
		}
	}

	/**
	 * @brief Clear all entries
	 */
	void Clear() {
		m_mItems.clear();
	}

	// Setters
	void SetTTL(unsigned int u) { m_uTTL = u; }
	// !Setters
	// Getters
	unsigned int GetTTL() { return m_uTTL; }
	// !Getters
private:
	typedef pair<unsigned long long, V> value;
	typedef typename map<K, value>::iterator iterator;
	map<K, value>	m_mItems;	//!< Map of cached items.  The value portion of the map is for the expire time
	unsigned int	m_uTTL;					//!< Default time-to-live duration
};

/**
 * @class CSmartPtr
 * @author prozac <prozac@rottenboy.com>
 * @brief This is a standard reference counting pointer.  Be careful not to have two of these point to the same raw pointer or one will be deleted while the other still thinks it is valid.
 */
template<typename T>
class CSmartPtr {
public:
	/**
	 * @brief Standard constructor, points to nothing
	 */
	CSmartPtr() {
		m_pType = NULL;
		m_puCount = NULL;
	}

	/**
	 * @brief Attach to an existing raw pointer, be CAREFUL not to have more than one CSmartPtr attach to the same raw pointer or bad things will happen
	 * @param pRawPtr The raw pointer to attach to
	 */
	CSmartPtr(T* pRawPtr) {
		m_pType = NULL;
		m_puCount = NULL;

		Attach(pRawPtr);
	}

	/**
	 * @brief Copy constructor, will copy the raw pointer and counter locations and increment the reference counter
	 * @param CopyFrom A reference of another CSmartPtr to copy from
	 */
	CSmartPtr(const CSmartPtr<T>& CopyFrom) {
		m_pType = NULL;
		m_puCount = NULL;

		*this = CopyFrom;
	}

	/**
	 * @brief Destructor will Release() the raw pointer and delete it if this was the last reference
	 */
	~CSmartPtr() {
		Release();
	}

	// Overloaded operators
	T& operator *() const { assert(m_pType); return *m_pType; }
	T* operator ->() const { assert(m_pType); return m_pType; }
	bool operator ==(T* rhs) { return (m_pType == rhs); }
	bool operator ==(const CSmartPtr<T>& rhs) { return (m_pType == *rhs); }

	/**
	 * @brief Attach() to a raw pointer
	 * @param pRawPtr The raw pointer to keep track of, ***WARNING*** Do _NOT_ allow more than one CSmartPtr keep track of the same raw pointer
	 * @return Reference to self
	 */
	CSmartPtr<T>& operator =(T* pRawPtr) { Attach(pRawPtr); return *this; }

	/**
	 * @brief Copies an existing CSmartPtr adding another reference to the counter
	 * @param CopyFrom A reference to another CSmartPtr to be copied
	 * @return Reference to self
	 */
	CSmartPtr<T>& operator =(const CSmartPtr<T>& CopyFrom) {
		if (&CopyFrom != this) {				// Check for assignment to self
			Release();							// Release the current pointer

			if (CopyFrom.IsNull()) {			// If the source raw pointer is null
				return *this;					// Then just bail out
			}

			m_pType = CopyFrom.m_pType;			// Make our pointers reference the same raw pointer and counter
			m_puCount = CopyFrom.m_puCount;

			assert(m_puCount);					// We now point to something valid, so increment the counter
			(*m_puCount)++;
		}

		return *this;
	}
	// !Overloaded operators

	/**
	 * @brief Implicit type conversion to bool for things like if (!ptr) {} and if (ptr) {}
	 * @return @see IsNull()
	 */
	operator bool() const {
		return !IsNull();
	}

	/**
	 * @brief Check to see if the underlying raw pointer is null
	 * @return Whether or not underlying raw pointer is null
	 */
	bool IsNull() const {
		return (m_pType == NULL);
	}

	/**
	 * @brief Attach to a given raw pointer, it will Release() the current raw pointer and assign the new one
	 * @param pRawPtr The raw pointer to keep track of, ***WARNING*** Do _NOT_ allow more than one CSmartPtr keep track of the same raw pointer
	 * @return Reference to self
	 */
	CSmartPtr<T>& Attach(T* pRawPtr) {
		if (pRawPtr != m_pType) {					// Check for assignment to self
			Release();								// Release the current pointer
			m_pType = pRawPtr;						// Point to the passed raw pointer

			if (m_pType) {							// If the passed pointer was valid
				m_puCount = new unsigned int(1);	// Create a new counter starting at 1 (us)
			}
		}

		return *this;
	}

	/**
	 * @brief Releases the underlying raw pointer and cleans up if we were the last reference to said pointer
	 */
	void Release() {
		if (m_pType) {				// Only release if there is something to be released
			assert(m_puCount);
			(*m_puCount)--;			// Decrement our counter

			if (!*m_puCount) {		// If we were the last reference to this pointer, then clean up
				delete m_puCount;
				delete m_pType;
			}

			m_pType = NULL;			// Get rid of our references
			m_puCount = NULL;
		}
	}

	// Getters
	T* GetPtr() const { return m_pType; }
	unsigned int GetCount() const { return (m_puCount) ? *m_puCount : 0; }
	// !Getters
private:
	T*				m_pType;	//!< Raw pointer to the class being referenced
	unsigned int*	m_puCount;	//!< Counter of how many CSmartPtr's are referencing the same raw pointer
};

#endif // !_UTILS_H

