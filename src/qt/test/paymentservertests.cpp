// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "paymentservertests.h"

#include "optionsmodel.h"
#include "paymentrequestdata.h"

#include "random.h"
#include "util.h"
#include "utilstrencodings.h"

#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include <QFileOpenEvent>
#include <QTemporaryFile>

X509 *parse_b64der_cert(const char* cert_data)
{
    std::vector<unsigned char> data = DecodeBase64(cert_data);
    assert(data.size() > 0);
    const unsigned char* dptr = &data[0];
    X509 *cert = d2i_X509(NULL, &dptr, data.size());
    assert(cert);
    return cert;
}


void PaymentServerTests::paymentServerTests()
{
}

void RecipientCatcher::getRecipient(SendCoinsRecipient r)
{
    recipient = r;
}
