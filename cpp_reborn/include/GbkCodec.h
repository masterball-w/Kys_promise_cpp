#pragma once
#include <string>

namespace GbkCodec {

/** GBK/CP936 bytes → UTF-8 (works without OS iconv). */
std::string toUtf8(const std::string& gbkBytes);

/** UTF-8 → GBK/CP936 bytes. */
std::string fromUtf8(const std::string& utf8Text);

}  // namespace GbkCodec
