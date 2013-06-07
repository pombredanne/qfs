#include "KrbService.h"

#include <krb5/krb5.h>

#include <string>
#include <algorithm>

namespace KFS
{

using std::string;
using std::max;

class KrbService::Impl
{
public:
    Impl()
        : mCtx(),
          mAuthCtx(),
          mServer(),
          mKeyTab(),
          mErrCode(0),
          mInitedFlag(false),
          mAuthInitedFlag(false),
          mServiceName(),
          mErrorMsg()
        {}
    ~Impl()
    {
        Impl::CleanupSelf();
    }
    const char* Init(
        const char* inServeiceNamePtr)
    {
        CleanupSelf();
        mErrorMsg.clear();
        mServiceName.clear();
        if (inServeiceNamePtr) {
            mServiceName = inServeiceNamePtr;
        }
        InitSelf();
        if (mErrCode) {
            mErrorMsg = ErrToStr(mErrCode);
            CleanupSelf();
        }
        return (mErrCode ? mErrorMsg.c_str() : 0);
    }
    const char* ApReq(
        const char* inDataPtr,
        int         inDataLen)
    {
        CleanupAuth();
        InitAuth();
        if (mErrCode) {
            mErrorMsg = ErrToStr(mErrCode);
        } else {
            krb5_data theData = { 0 };
            theData.length = max(0, inDataLen);
            theData.data   = const_cast<char*>(inDataPtr);
            krb5_flags   theReqOptions = { 0 };
            krb5_ticket* theTicket     = 0;
            krb5_error_code theErr = krb5_rd_req(
                mCtx,
                &mAuthCtx,
                &theData,
                mServer,
                mKeyTab,
                &theReqOptions,
                &theTicket
            );
            krb5_free_ticket(mCtx, theTicket);
            if (! theErr) {
                return 0;
            }
            mErrorMsg = ErrToStr(theErr);
        }
        return mErrorMsg.c_str();
    }

private:
    string            mKeyTabFileName;
    krb5_context      mCtx;
    krb5_auth_context mAuthCtx;
    krb5_principal    mServer;
    krb5_keytab       mKeyTab;
    krb5_error_code   mErrCode;
    bool              mInitedFlag;
    bool              mAuthInitedFlag;
    string            mServiceName;
    string            mErrorMsg;

    void InitSelf()
    {
        mErrCode = krb5_init_context(&mCtx);
        if (mErrCode) {
            return;
        }
	mErrCode = krb5_sname_to_principal(
            mCtx, 0, mServiceName.c_str(), KRB5_NT_SRV_HST, &mServer);
        if (mErrCode) {
            return;
        }
        mKeyTab = 0;
        mErrCode = mKeyTabFileName.empty() ?
            krb5_kt_default(mCtx, &mKeyTab) :
            krb5_kt_resolve(mCtx, mKeyTabFileName.c_str(), &mKeyTab);
    }
    void InitAuth()
    {
        InitAuthSelf();
        if (mErrCode) {
            CleanupAuth();
        }
    }
    void InitAuthSelf()
    {
        if (! mInitedFlag) {
            if (! mErrCode) {
                mErrCode = KRB5_CONFIG_BADFORMAT;
            }
            return;
        }
        mErrCode = krb5_auth_con_init(mCtx, &mAuthCtx);
        if (mErrCode) {
            return;
        }
        mAuthInitedFlag = true;
        krb5_rcache theRCachePtr = 0;
        mErrCode = krb5_auth_con_getrcache(mCtx, mAuthCtx, &theRCachePtr);
        if (mErrCode) {
            return;
        }
	if (! theRCachePtr)  {
            mErrCode = krb5_get_server_rcache(
                mCtx, krb5_princ_component(mCtx, mServer, 0), &theRCachePtr);
            if (mErrCode) {
                return;
            }
        }
        mErrCode = krb5_auth_con_setrcache(mCtx, mAuthCtx, theRCachePtr);
        if (mErrCode) {
            return;
        }
    }
    krb5_error_code CleanupSelf()
    {
        if (! mInitedFlag) {
            return 0;
        }
        krb5_error_code theErr = CleanupAuth();
        mInitedFlag = false;
        if (mKeyTab) {
            krb5_error_code const theCloseErr = krb5_kt_close(mCtx, mKeyTab);
            mKeyTab = 0;
            if (! theErr && theCloseErr) {
                theErr = theCloseErr;
            }
        }
        krb5_free_context(mCtx);
        return theErr;
    }
    krb5_error_code CleanupAuth()
    {
        if (! mInitedFlag || ! mAuthInitedFlag) {
            return 0;
        }
        mAuthInitedFlag = false;
        return krb5_auth_con_free(mCtx, mAuthCtx);
    }
    string ErrToStr(
        krb5_error_code inErrCode) const
    {
        if (! inErrCode) {
            return string();
        }
        if ( ! mCtx) {
            return string("no kerberos context");
        }
        const char* const theMsgPtr = krb5_get_error_message(mCtx, inErrCode);
        return string(theMsgPtr);
    }
};

KrbService::KrbService()
    : mImpl(*(new Impl()))
{
}

KrbService::~KrbService()
{
    delete &mImpl;
}

    const char*
KrbService::Init(
        const char* inServeiceNamePtr)
{
    return mImpl.Init(inServeiceNamePtr);
} 

}
