#include "config.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "email/lib.h"

/**
 * email_pack_flags - Pack the Email flags into a uint32_t
 * @param e Email to pack
 * @retval num uint32_t of packed flags
 *
 * @note Order of packing must match email_unpack_flags()
 */
uint32_t email_pack_flags(const struct Email *e)
{
  if (!e)
    return 0;

  // clang-format off
  return e->security +
        (e->expired    << 16) +
        (e->flagged    << 17) +
        (e->mime       << 18) +
        (e->old        << 19) +
        (e->read       << 20) +
        (e->replied    << 21) +
        (e->superseded << 22) +
        (e->trash      << 23);
  // clang-format on
}

/**
 * email_unpack_flags - Unpack the Email flags from a uint32_t
 * @param e      Email to unpack into
 * @param packed Packed flags
 *
 * @note Order of packing must match email_pack_flags()
 */
void email_unpack_flags(struct Email *e, uint32_t packed)
{
  if (!e)
    return;

  // clang-format off
  e->security   = (packed & ((1 << 16) - 1)); // bits 0-15
  e->expired    = (packed & (1 << 16));
  e->flagged    = (packed & (1 << 17));
  e->mime       = (packed & (1 << 18));
  e->old        = (packed & (1 << 19));
  e->read       = (packed & (1 << 20));
  e->replied    = (packed & (1 << 21));
  e->superseded = (packed & (1 << 22));
  e->trash      = (packed & (1 << 23));
  // clang-format on
}

/**
 * email_pack_timezone - Pack the Email timezone into a uint32_t
 * @param e Email to pack
 * @retval num uint32_t of packed timezone
 *
 * @note Order of packing must match email_unpack_timezone()
 */
uint32_t email_pack_timezone(const struct Email *e)
{
  if (!e)
    return 0;

  return e->zhours + (e->zminutes << 5) + (e->zoccident << 11);
}

/**
 * email_unpack_timezone - Unpack the Email timezone from a uint32_t
 * @param e      Email to unpack into
 * @param packed Packed timezone
 *
 * @note Order of packing must match email_pack_timezone()
 */
void email_unpack_timezone(struct Email *e, uint32_t packed)
{
  if (!e)
    return;

  // clang-format off
  e->zhours    =  (packed       & ((1 << 5) - 1)); // bits 0-4 (5)
  e->zminutes  = ((packed >> 5) & ((1 << 6) - 1)); // bits 5-10 (6)
  e->zoccident =  (packed       &  (1 << 11));     // bit  11 (1)
  // clang-format on
}

/**
 * body_pack_flags - Pack the Body flags into a uint32_t
 * @param b Body to pack
 * @retval num uint32_t of packed flags
 *
 * @note Order of packing must match body_unpack_flags()
 */
uint32_t body_pack_flags(const struct Body *b)
{
  if (!b)
    return 0;

  // clang-format off
  uint32_t packed = b->type +
                   (b->encoding      <<  4) +
                   (b->disposition   <<  7) +
                   (b->badsig        <<  9) +
                   (b->force_charset << 10) +
                   (b->goodsig       << 11) +
                   (b->noconv        << 12) +
                   (b->use_disp      << 13) +
                   (b->warnsig       << 14);
  // clang-format on
#ifdef USE_AUTOCRYPT
  packed += (b->is_autocrypt << 15);
#endif

  return packed;
}

/**
 * body_unpack_flags - Unpack the Body flags from a uint32_t
 * @param b      Body to unpack into
 * @param packed Packed flags
 *
 * @note Order of packing must match body_pack_flags()
 */
void body_unpack_flags(struct Body *b, uint32_t packed)
{
  if (!b)
    return;

  // clang-format off
  b->type         =  (packed       & ((1 << 4) - 1)); // bits 0-3 (4)
  b->encoding     = ((packed >> 4) & ((1 << 3) - 1)); // bits 4-6 (3)
  b->disposition  = ((packed >> 7) & ((1 << 2) - 1)); // bits 7-8 (2)

  b->badsig        = (packed & (1 <<  9));
  b->force_charset = (packed & (1 << 10));
  b->goodsig       = (packed & (1 << 11));
  b->noconv        = (packed & (1 << 12));
  b->use_disp      = (packed & (1 << 13));
  b->warnsig       = (packed & (1 << 14));
#ifdef USE_AUTOCRYPT
  b->is_autocrypt  = (packed & (1 << 15));
#endif
  // clang-format on
}

void dump(const struct Body *b)
{
  printf("%02d:%d:%d:%d%d%d%d%d%d%d\n", b->type, b->encoding, b->disposition,
         b->badsig, b->force_charset, b->goodsig, b->noconv, b->use_disp,
         b->warnsig, b->is_autocrypt);
}

void test_body_pack_flags(void)
{
  struct Body b = { 0 };

  unsigned short *top = (unsigned short *) &b;
  for (int i = 0; i < (1 << 16); i++)
  {
    unsigned short num_before = i;
    *top = num_before;
    // dump(&b);
    uint32_t packed = 0;

    packed = body_pack_flags(&b);
    memset(&b, 0, sizeof(b));
    body_unpack_flags(&b, packed);

    unsigned short num_after = *top;

    if (num_before != num_after)
    {
      printf("%6hu, %6hu\n", num_before, num_after);
    }
  }
}

void test_email_pack_flags(void)
{
  struct Email e = { 0 };

  unsigned int *top = (unsigned int *) &e;

  for (int i = 0; i < (1 << 24); i++)
  {
    unsigned int num_before = i;
    *top = num_before;
    uint32_t packed = 0;

    packed = email_pack_flags(&e);
    memset(&e, 0, sizeof(e));
    email_unpack_flags(&e, packed);

    unsigned int num_after = *top;

    if (num_before != num_after)
    {
      printf("%6u, %6u\n", num_before, num_after);
    }
  }
}

void test_email_pack_timezone(void)
{
  struct Email e = { 0 };

  unsigned int *top = (unsigned int *) ((unsigned char *) &e + 3);

  for (int i = 0; i < (1 << 12); i++)
  {
    e.zhours    =  (i        & ((1 << 5) - 1));
    e.zminutes  = ((i >> 5)  & ((1 << 6) - 1));
    e.zoccident = ((i >> 11) & 1);

    unsigned int num_before = *top;

    uint32_t packed = email_pack_timezone(&e);
    memset(&e, 0, sizeof(e));
    email_unpack_timezone(&e, packed);

    unsigned int num_after = *top;

    if (num_before != num_after)
    {
      printf("%6u, %6u\n", num_before, num_after);
    }
  }
}

int main()
{
  test_body_pack_flags();
  test_email_pack_flags();
  test_email_pack_timezone();

  return 0;
}
