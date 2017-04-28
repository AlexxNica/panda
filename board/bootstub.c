#ifdef STM32F4
  #define PANDA
  #include "stm32f4xx.h"
#else
  #include "stm32f2xx.h"
#endif

#include "early.h"
#include "libc.h"

#include "crypto/rsa.h"
#include "crypto/sha.h"

#include "obj/cert.h"

void lock_bootloader() {
  if (FLASH->OPTCR & FLASH_OPTCR_nWRP_0) {
    FLASH->OPTKEYR = 0x08192A3B;
    FLASH->OPTKEYR = 0x4C5D6E7F;

    // write protect the bootloader
    FLASH->OPTCR &= ~FLASH_OPTCR_nWRP_0;

    // OPT program
    FLASH->OPTCR |= FLASH_OPTCR_OPTSTRT;
    while (FLASH->SR & FLASH_SR_BSY);

    // relock it
    FLASH->OPTCR |= FLASH_OPTCR_OPTLOCK;

    // reset
    NVIC_SystemReset();
  }
}

void __initialize_hardware_early() {
  //lock_bootloader();
  early();
}

void fail() {
  enter_bootloader_mode = ENTER_BOOTLOADER_MAGIC;
  NVIC_SystemReset();
}

int main() {
  clock_init();

  // validate length
  int len = _app_start[0];
  if (len < 8) fail();

  // compute SHA hash
  char digest[SHA_DIGEST_SIZE];
  SHA_hash(&_app_start[1], len-4, digest);

  // verify RSA signature
  if (RSA_verify(&release_rsa_key, ((void*)&_app_start[0]) + len, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
  }

  // allow debug cert for now
  if (RSA_verify(&debug_rsa_key, ((void*)&_app_start[0]) + len, RSANUMBYTES, digest, SHA_DIGEST_SIZE)) {
    goto good;
  }

// here is a failure
  fail();
good:
  // jump to flash
  ((void(*)()) _app_start[1])();
  return 0;
}

