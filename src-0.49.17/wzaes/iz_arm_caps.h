/*
 * SPDX-License-Identifier: MIT-0
 * Copyright (c) 2026 Jeffrey H. Johnson <johnsonjh.dev@gmail.com>
 */

#ifndef IZ_ARM_CAPS_H
# define IZ_ARM_CAPS_H
# if defined(__GNUC__) && defined(__aarch64__) && !defined(__AARCH64EB__) \
    && (defined(__clang__) || __GNUC__ >= 7)
#  define IZ_ARM_CRYPTO_SUPPORTED 1
#  define IZ_ARM_AES_BIT (1UL << 3)
#  define IZ_ARM_PMULL_BIT (1UL << 4)
#  define IZ_ARM_SHA1_BIT (1UL << 5)
#  define IZ_ARM_SHA2_BIT (1UL << 6)
#  define IZ_ARM_CRC_BIT (1UL << 7)
#  if defined(__linux__)
#   include <sys/auxv.h>
#  elif defined(__FreeBSD__) || defined(__OpenBSD__)
#   include <sys/auxv.h>
#  elif defined(__APPLE__) && defined(__MACH__)
#   include <sys/sysctl.h>
#   include <sys/types.h>
#  endif /* if defined( __linux__ ) */
static unsigned long
iz_arm_capabilities (void)
{
#  if defined(__linux__)
  return getauxval (AT_HWCAP);

#  elif defined(__FreeBSD__) || (defined(__OpenBSD__) && (OpenBSD >= 202409))
  unsigned long caps = 0;
  if (elf_aux_info (AT_HWCAP, &caps, sizeof (caps)) != 0)
    {
      return 0;
    }

  return caps;

#  elif defined(__APPLE__) && defined(__MACH__)
  unsigned long caps = 0;
  int v = 0;
  size_t n = sizeof (v);
  /* macOS arm64 guarantees AES, PMULL, SHA1 and SHA256.  Only the
   * optional CRC extension needs a separate runtime query. */
  caps = IZ_ARM_AES_BIT | IZ_ARM_PMULL_BIT | IZ_ARM_SHA1_BIT | IZ_ARM_SHA2_BIT;
  if (sysctlbyname ("hw.optional.armv8_crc32", &v, &n, NULL, 0) == 0 && v)
    {
      caps |= IZ_ARM_CRC_BIT;
    }

  return caps;

#  else /* if defined( __linux__ ) */
  /* Never infer ISA availability merely from the compiler target. */
  return 0;

#  endif /* if defined( __linux__ ) */
}
static __attribute__ ((unused)) int
iz_arm_has (unsigned long bit)
{
  static unsigned long checked = 0; /* cached caps, distinguished by a flag */
  static int initialized = 0;

  if (!__atomic_load_n (&initialized, __ATOMIC_ACQUIRE))
    {
      unsigned long caps = iz_arm_capabilities ();
      __atomic_store_n (&checked, caps, __ATOMIC_RELAXED);
      __atomic_store_n (&initialized, 1, __ATOMIC_RELEASE);
    }

  return (__atomic_load_n (&checked, __ATOMIC_RELAXED) & bit) != 0;
}
# endif /* IZ_ARM_CRYPTO_SUPPORTED */
#endif /* IZ_ARM_CAPS_H */
