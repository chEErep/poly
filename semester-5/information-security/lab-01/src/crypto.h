#ifndef LAB_CRYPTO_H
#define LAB_CRYPTO_H

#include <windows.h>
#include <wincrypt.h>
#define LAB_AES_IV_BYTES 16u
// Контекст хранит «ручки» CryptoAPI, а не байты приватного ключа
typedef struct lab_crypto_context {
    HCRYPTPROV provider;
    HCRYPTKEY exchange_key; // РСА ключ для обмена
    HCRYPTKEY session_key; //АЕС ключ
} lab_crypto_context;

// Перед первым вызовом контекст должен быть инициализирован как {0}
int lab_crypto_open(lab_crypto_context *context);
int lab_crypto_generate_exchange_key(lab_crypto_context *context);
int lab_crypto_export_public_key(lab_crypto_context *context,
                                 BYTE *buffer,
                                 DWORD *buffer_length);

int lab_crypto_create_session_key(lab_crypto_context *context,
                                  const BYTE *public_blob,
                                  DWORD public_blob_length,
                                  BYTE *wrapped_blob,
                                  DWORD *wrapped_blob_length);

int lab_crypto_accept_session_key(lab_crypto_context *context,
                                  const BYTE *wrapped_blob,
                                  DWORD wrapped_blob_length);
int lab_crypto_encrypt_message(lab_crypto_context *context,
                               const BYTE *plaintext,
                               DWORD plaintext_length,
                               BYTE *output,
                               DWORD output_capacity,
                               DWORD *output_length);

int lab_crypto_decrypt_message(lab_crypto_context *context,
                               const BYTE *message,
                               DWORD message_length,
                               BYTE *output,
                               DWORD output_capacity,
                               DWORD *output_length);
void lab_crypto_close(lab_crypto_context *context);

#endif
