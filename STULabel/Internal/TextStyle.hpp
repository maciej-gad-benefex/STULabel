// Copyright 2017–2018 Stephan Tolksdorf

#import "STULabel/STUTextAttributes.h"
#import "STULabel/STUTextFlags.h"

#import "Color.hpp"
#import "Common.hpp"
#import "GlyphSpan.hpp"
#import "Rect.hpp"
#import "ThreadLocalAllocator.hpp"
#import "TextFlags.hpp"
#import "TextFrameCompactIndex.hpp"

#import "stu/Ref.hpp"

#import <CoreText/CoreText.h>

#import <stdalign.h>

@class STUTextHighlightStyle;

namespace stu_label {

struct FontIndex : Comparable<FontIndex> {
  UInt16 value;

  // We want a trivial default constructor.
  STU_INLINE_T
  FontIndex() = default;

  explicit STU_CONSTEXPR_T
  FontIndex(UInt16 value) : value{value} {}

  STU_CONSTEXPR_T
  friend bool operator==(FontIndex lhs, FontIndex rhs) { return lhs.value == rhs.value; }
  STU_CONSTEXPR_T
  friend bool operator<(FontIndex lhs, FontIndex rhs) { return lhs.value < rhs.value; }
};

struct ColorIndex : Comparable<ColorIndex> {
  UInt16 value;

  // We want a trivial default constructor.
  STU_INLINE_T
  ColorIndex() = default;

  explicit STU_CONSTEXPR_T
  ColorIndex(UInt16 value) : value{value} {}

  static constexpr UInt16 overrideColorCount = 2;
  static constexpr UInt16 highlightColorCount = 7;
  static constexpr UInt16 fixedColorCount = 1 + overrideColorCount + highlightColorCount;
  static constexpr Range<UInt16> fixedColorIndexRange = {1, 1 + fixedColorCount};
  static constexpr UInt16 highlightColorStartIndex = fixedColorIndexRange.end - highlightColorCount;

  static const ColorIndex reserved; // = 0
  static const ColorIndex black; // = fixedColorStartIndex

  static const ColorIndex overrideTextColor;
  static const ColorIndex overrideLinkColor;

  STU_CONSTEXPR_T
  friend bool operator==(ColorIndex lhs, ColorIndex rhs) { return lhs.value == rhs.value; }
  STU_CONSTEXPR_T
  friend bool operator<(ColorIndex lhs, ColorIndex rhs) { return lhs.value < rhs.value; }
};
constexpr ColorIndex ColorIndex::reserved{0};
constexpr ColorIndex ColorIndex::black{ColorIndex::fixedColorIndexRange.start};
constexpr ColorIndex ColorIndex::overrideTextColor{ColorIndex::fixedColorIndexRange.start + 1};
constexpr ColorIndex ColorIndex::overrideLinkColor{ColorIndex::fixedColorIndexRange.start + 2};

} // stu_label

template <>
class stu::OptionalValueStorage<stu_label::ColorIndex> {
public:
  stu_label::ColorIndex value_{stu_label::ColorIndex::reserved};
  STU_CONSTEXPR bool hasValue() const noexcept { return value_ != stu_label::ColorIndex::reserved; }
  STU_CONSTEXPR void clearValue() noexcept { value_ = stu_label::ColorIndex::reserved; }
  STU_CONSTEXPR void constructValue(stu_label::ColorIndex value) { value_ = value; }
};

namespace stu_label {

struct UnderlineStyle : Parameter<UnderlineStyle, UInt16> {
  using Parameter::Parameter;

  /* implicit */ STU_CONSTEXPR
  UnderlineStyle(NSUnderlineStyle underlineStyle)
  : UnderlineStyle(static_cast<UInt16>(underlineStyle)) {}

  STU_CONSTEXPR
  explicit operator NSUnderlineStyle() const {
    return static_cast<NSUnderlineStyle>(value);
  }
};

struct StrikethroughStyle : Parameter<StrikethroughStyle, UInt16> {
  using Parameter::Parameter;
  
  /* implicit */ STU_CONSTEXPR
  StrikethroughStyle(NSUnderlineStyle strikethroughStyle)
  : StrikethroughStyle(static_cast<UInt16>(strikethroughStyle)) {}

  STU_CONSTEXPR
  explicit operator NSUnderlineStyle() const {
    return static_cast<NSUnderlineStyle>(value);
  }
};

struct TextStyle {

  struct BitSize {
    static constexpr int flags = STUTextFlagsBitSize;
    static constexpr int offsetFromPreviousDiv4 = 5;
    static constexpr int offsetToNextDiv4 = 5;
    struct Small {
      static constexpr int stringIndex = 26;
      static constexpr int font = 8;
      static constexpr int color = 8;
    };
    struct Big {
      static constexpr int stringIndex = 31;
    };
  };
  struct BitIndex {
    static constexpr int isBig = 0;
    static constexpr int flags = 1;
    static constexpr int offsetFromPreviousDiv4 = flags + BitSize::flags;
    static constexpr int isOverride = offsetFromPreviousDiv4 + BitSize::offsetFromPreviousDiv4;
    static constexpr int offsetToNextDiv4 = isOverride + 1;
    static constexpr int stringIndex = offsetToNextDiv4 + BitSize::offsetToNextDiv4;
    struct Small {
      static constexpr int font = stringIndex + BitSize::Small::stringIndex;
      static constexpr int color = font + BitSize::Small::font;
      static_assert(color + BitSize::Small::color == 64);
    };
    struct Big {
      static_assert(stringIndex + BitSize::Big::stringIndex <= 64);
    };
  };

  static constexpr Int32 maxSmallStringIndex = (1 << BitSize::Small::stringIndex) - 1;
  static constexpr UInt16 maxSmallFontIndex = (1 << BitSize::Small::font) - 1;
  static constexpr UInt16 maxSmallColorIndex = (1 << BitSize::Small::color) - 1;

  __attribute__((aligned(4), packed)) UInt64 bits;

  struct Big;

  STU_CONSTEXPR
  explicit TextStyle(UInt64 bits) : bits(bits) {}

protected:
  static constexpr int bigSize = 12; // sizeof(Big)

  TextStyle() = delete;

  explicit TextStyle(const TextStyle&) = default;
  TextStyle& operator=(const TextStyle&) = default;

public:

  STU_INLINE
  bool isBig() const {
    static_assert(BitIndex::isBig == 0);
    return STU_UNLIKELY(bits & 1);
  }

  STU_INLINE
  bool isOverrideStyle() const {
    return bits & (1 << BitIndex::isOverride);
  }

  STU_INLINE
  TextFlags flags() const {
    return static_cast<TextFlags>((bits >> BitIndex::flags) & ((1 << BitSize::flags) - 1));
  }

  struct IsOverrideIsLinkIndex {
    /// A value in [0, 3]
    UInt index;

    bool isOverride() const { return index & 1; }
    bool isLink() const { return index & 2; }
  };

  STU_INLINE
  IsOverrideIsLinkIndex isOverride_isLink() const {
    static_assert(BitIndex::flags == 1 && int(TextFlags::hasLink) == 1);
    return {narrow_cast<UInt>(  ((bits >> BitIndex::isOverride) & 1)
                              | (bits & (1 << BitIndex::flags)))};
  }

  STU_INLINE
  Int32 stringIndex() const {
    return stu::narrow_cast<Int32>(bits >> BitIndex::stringIndex)
         & stringIndexMask[bits & 1];
  }

  STU_INLINE
  void setStringIndex(Int32 value) {
    const bool isBig = this->isBig();
    const UInt32 maxValue = isBig ? maxSmallStringIndex : INT32_MAX;
    const UInt64 mask = ~(UInt64{maxValue} << BitIndex::stringIndex);
    STU_PRECONDITION(sign_cast(value) <= maxSmallStringIndex);
    bits = (bits & mask) | (UInt64(value) << BitIndex::stringIndex);
  }

  /// Returns *this if this is the first style or an overrideStyle.
  STU_INLINE
  const TextStyle& previous() const {
    const UInt offsetDiv4 = narrow_cast<UInt>((bits >> BitIndex::offsetFromPreviousDiv4)
                                              & ((1 << BitSize::offsetFromPreviousDiv4) - 1));
    return *reinterpret_cast<const TextStyle*>(reinterpret_cast<const Byte*>(this) - 4*offsetDiv4);
  }

  /// Returns *this if this is the last style or an overrideStyle.
  STU_INLINE
  const TextStyle& next() const {
    const UInt offsetDiv4 = narrow_cast<UInt>((bits >> BitIndex::offsetToNextDiv4)
                                              & ((1 << BitSize::offsetToNextDiv4) - 1));
    return *reinterpret_cast<const TextStyle*>(reinterpret_cast<const Byte*>(this) + 4*offsetDiv4);
  }

  const TextStyle& styleForStringIndex(Int32 index) const;


  STU_INLINE
  FontIndex fontIndex() const;

  STU_INLINE
  ColorIndex colorIndex() const;

  struct LinkInfo {
    __unsafe_unretained id attribute __attribute__((aligned(4), packed));
  };
  static_assert(alignof(LinkInfo) == 4 && sizeof(LinkInfo)%4 == 0);

  struct BackgroundInfo {
    const STUBackgroundAttribute* __unsafe_unretained
      stuAttribute __attribute__((aligned(4), packed));

    Optional<ColorIndex> colorIndex;
    Optional<ColorIndex> borderColorIndex;
  };
  static_assert(alignof(BackgroundInfo) && sizeof(BackgroundInfo)%4 == 0);

  struct ShadowInfo {
    Float32 offsetX;
    Float32 offsetY;
    Float32 blurRadius;
    ColorIndex colorIndex;

    STU_INLINE CGPoint offset() const { return {offsetX, offsetY}; }

    bool operator==(const ShadowInfo& other) const {
      return offsetX    == other.offsetX
          && offsetY    == other.offsetY
          && blurRadius == other.blurRadius
          && colorIndex == other.colorIndex;
    }
    bool operator!=(const ShadowInfo& other) const { return !(*this == other); }
  };
  static_assert(sizeof(ShadowInfo)%4 == 0);

  struct UnderlineInfo {
    UnderlineStyle style;
    Optional<ColorIndex> colorIndex; ///< `none` is a placeholder for the text foreground color.

    Float32 originalFontUnderlineOffset;
    Float32 originalFontUnderlineThickness;
  };

  struct StrikethroughInfo {
    StrikethroughStyle style;
    Optional<ColorIndex> colorIndex; ///< `none` is a placeholder for the text foreground color.
    Float32 originalFontStrikethroughThickness;
  };

  struct StrokeInfo {
    /// > 0
    Float32 strokeWidth;
    Optional<ColorIndex> colorIndex; ///< `none` is a placeholder for the text foreground color.
    bool doNotFill;
  };
  static_assert(sizeof(StrokeInfo)%4 == 0);

  struct AttachmentInfo {
    const STUTextAttachment* __unsafe_unretained attribute __attribute__((aligned(4), packed));
  };
  static_assert(alignof(AttachmentInfo) == 4 && sizeof(AttachmentInfo)%4 == 0);

  struct BaselineOffsetInfo {
    Float32 baselineOffset;
  };

  bool hasLink() const { return !!(flags() & TextFlags::hasLink);}
  bool hasAttachment() const { return !!(flags() & TextFlags::hasAttachment);}
  bool hasBaselineOffset() const { return !!(flags() & TextFlags::hasBaselineOffset);}

  STU_INLINE 
  const LinkInfo* linkInfo() const {
    return down_cast<const LinkInfo*>(this->info(TextFlags::hasLink));
  }

  STU_INLINE
  const BackgroundInfo* backgroundInfo() const {
    return down_cast<const BackgroundInfo*>(info(TextFlags::hasBackground));
  }
  STU_INLINE
  const ShadowInfo* shadowInfo() const {
    return down_cast<const ShadowInfo*>(info(TextFlags::hasShadow));
  }
  STU_INLINE
  const UnderlineInfo* underlineInfo() const {
    return down_cast<const UnderlineInfo*>(info(TextFlags::hasUnderline));
  }
  STU_INLINE
  const StrikethroughInfo* strikethroughInfo() const {
    return down_cast<const StrikethroughInfo*>(info(TextFlags::hasStrikethrough));
  }
  STU_INLINE
  const StrokeInfo* strokeInfo() const {
    return down_cast<const StrokeInfo*>(info(TextFlags::hasStroke));
  }
  STU_INLINE
  const AttachmentInfo* attachmentInfo() const {
    return down_cast<const AttachmentInfo*>(this->info(TextFlags::hasAttachment));
  }
  STU_INLINE
  const Float32 baselineOffset() const {
    if (hasBaselineOffset()) {
      const Float32 value = baselineOffsetInfo()->baselineOffset;
      STU_ASSUME(value != 0);
      return value;
    }
    return 0;
  }
  STU_INLINE
  const BaselineOffsetInfo* baselineOffsetInfo() const {
    return down_cast<const BaselineOffsetInfo*>(this->info(TextFlags::hasBaselineOffset));
  }

  static constexpr UInt16 maxSize = bigSize
                                  + sizeof(LinkInfo)
                                  + sizeof(BackgroundInfo)
                                  + sizeof(ShadowInfo)
                                  + sizeof(UnderlineInfo)
                                  + sizeof(StrikethroughInfo)
                                  + sizeof(StrokeInfo)
                                  + sizeof(AttachmentInfo)
                                  + sizeof(BaselineOffsetInfo);

  const void* componentInfo(TextFlags component) const {
    STU_PRECONDITION(isPowerOfTwo(static_cast<UnderlyingType<TextFlags>>(component)));
    return info(component);
  }

  STU_CONSTEXPR
  static Int sizeOfTerminatorWithStringIndex(Int32 stringIndex) {
    return stringIndex > maxSmallStringIndex ? bigSize : 8;
  }

  static void writeTerminatorWithStringIndex(Int32, const Byte* previousTextStyle,
                                             ArrayRef<Byte> buffer);

private:
  friend class TextStyleOverride;

  static const Int32 stringIndexMask[2];
  static const UInt8 infoOffsets[256];

  STU_INLINE
  const void* nonnullOwnInfo(TextFlags component) const {
    static_assert(BitIndex::flags + TextFlagsBitSize <= 32);
    static_assert(BitIndex::flags == 1);
    const UInt32 flagBit = implicit_cast<UInt32>(static_cast<UInt16>(component)) << BitIndex::flags;
    const UInt index = narrow_cast<UInt>(bits & (flagBit - 1));
    STU_DEBUG_ASSERT(index < arrayLength(infoOffsets));
    const void* const p = reinterpret_cast<const Byte*>(this) + infoOffsets[index];
    STU_ASSUME(p != nullptr);
    return p;
  }

  const void* nonnullInfoFromOverride(TextFlags component) const;

  STU_INLINE
  const void* info(TextFlags component) const {
    static_assert(BitIndex::flags + TextFlagsBitSize <= 32);
    const UInt32 flagBit = implicit_cast<UInt32>(static_cast<UInt16>(component)) << BitIndex::flags;
    if (!(bits & flagBit)) return nullptr;
    return !isOverrideStyle() ? nonnullOwnInfo(component)
                              : nonnullInfoFromOverride(component);
  }
};

struct TextStyle::Big : TextStyle {
  FontIndex fontIndex;
  ColorIndex colorIndex;

  STU_CONSTEXPR
  explicit Big(UInt64 bits, FontIndex fontIndex, ColorIndex colorIndex)
  : TextStyle{bits}, fontIndex{fontIndex}, colorIndex{colorIndex} {}
};
static_assert(sizeof(TextStyle::Big) == 12, "TextStyle::bigSize needs to be adjusted.");

STU_INLINE FontIndex TextStyle::fontIndex() const {
  return !isBig()
       ? FontIndex{narrow_cast<UInt16>((bits >> BitIndex::Small::font) & maxSmallFontIndex)}
       : down_cast<const Big&>(*this).fontIndex;
}

STU_INLINE ColorIndex TextStyle::colorIndex() const {
  return !isBig()
       ? ColorIndex{narrow_cast<UInt16>((bits >> BitIndex::Small::color) & maxSmallColorIndex)}
       : down_cast<const Big&>(*this).colorIndex;
}

STU_INLINE
void TextStyle::writeTerminatorWithStringIndex(Int32 stringIndex, const Byte* previousStyle,
                                               ArrayRef<Byte> buffer)
{
  const bool isBig = stringIndex > maxSmallStringIndex;
  STU_PRECONDITION(buffer.count() == sizeOfTerminatorWithStringIndex(stringIndex));
  const Int offsetFromPrevious = buffer.begin() - reinterpret_cast<const Byte*>(previousStyle);
  STU_ASSERT(0 <= offsetFromPrevious
             && offsetFromPrevious <= ((1 << BitSize::offsetFromPreviousDiv4) - 1)*4);
  STU_ASSERT(offsetFromPrevious%4 == 0);
  const UInt offsetFromPreviousDiv4 = sign_cast(offsetFromPrevious)/4;
  const UInt64 bits = isBig
                    | (UInt64{offsetFromPreviousDiv4}
                       << TextStyle::BitIndex::offsetFromPreviousDiv4)
                    | (UInt64{sign_cast(stringIndex)} << TextStyle::BitIndex::stringIndex);
  if (!isBig) {
    new (buffer.begin()) TextStyle{bits};
  } else {
    new (buffer.begin()) TextStyle::Big{bits, FontIndex{0}, ColorIndex{0}};
  }
}


struct TextFrame;
class TextFrameDrawingOptions;
struct TextHighlightStyle;

class TextStyleOverride {
public:
  const Range<Int32> drawnLineRange;
  const Range<Int32> drawnRangeInOriginalString;
  const Range<TextFrameCompactIndex> drawnRange;
  const Range<Int32> overrideRangeInOriginalString; ///< Subrange of drawnRangeInOriginalString
  const Range<TextFrameCompactIndex> overrideRange; ///< Subrange of drawnRange
  const TextFlags flagsMask;
  const TextFlags flags;
  const Optional<ColorIndex> textColorIndex;
private:
  TextStyle::Big style_;
  const TextStyle* __nullable overriddenStyle_;
public:
  STU_INLINE const TextStyle& style() const { return style_; }
  STU_INLINE const TextStyle* __nullable overriddenStyle() const { return overriddenStyle_; }

  Optional<const TextHighlightStyle&> const highlightStyle;
private:
  const void* __nullable styleInfos_[6];

public:
  TextStyleOverride(const TextFrame&,
                    Range<TextFrameIndex> drawnRange,
                    Optional<const TextFrameDrawingOptions&> options);

  TextStyleOverride(Range<Int32> drawnLineRange,
                    Range<Int32> drawnRangeInOriginalString,
                    Range<TextFrameCompactIndex> drawnRange);

  void applyTo(const TextStyle& style);

private:
  static TextStyleOverride create(const TextFrame&, Range<TextFrameIndex> drawnRange,
                                  Optional<const TextFrameDrawingOptions&> options);


  TextStyleOverride(Range<Int32> drawnLineRange,
                    Range<Int32> drawnRangeInOriginalString,
                    Range<TextFrameCompactIndex> drawnRange,
                    Range<Int32> overrideRangeInOriginalString,
                    Range<TextFrameCompactIndex> overrideRange,
                    TextFlags flagsMask,
                    TextFlags flags,
                    Optional<ColorIndex> textColorIndex,
                    Optional<const TextHighlightStyle&> highlightStyle);

  friend const void* TextStyle::nonnullInfoFromOverride(TextFlags) const;
};

STU_INLINE
const void* TextStyle::nonnullInfoFromOverride(TextFlags component) const {
  STU_DISABLE_CLANG_WARNING("-Winvalid-offsetof")
  const UInt offset = offsetof(TextStyleOverride, style_);
  STU_REENABLE_CLANG_WARNING
  const TextStyleOverride* const so =
    reinterpret_cast<const TextStyleOverride*>(reinterpret_cast<const Byte*>(this) - offset);
  static_assert(static_cast<Int>(TextFlags::hasBackground) == 2);
  static_assert(static_cast<Int>(TextFlags::hasLink) == 1);
  // - 1 because hasBackground is the first overridable component, cf. TextStyleOverride::applyTo
  const int index = __builtin_ctz(static_cast<UInt16>(component)) - 1;
  STU_DEBUG_ASSERT(0 <= index && index < arrayLength(so->styleInfos_));
  const void* const pointer = so->styleInfos_[index];
  STU_ASSUME(pointer != nullptr);
  return pointer;
}

struct TextStyleSpan {
  const TextStyle* firstStyle;
  const TextStyle* terminatorStyle;

  STU_INLINE
  const Byte* dataBegin() const { return reinterpret_cast<const Byte*>(firstStyle); };

  STU_INLINE
  ArrayRef<const Byte> dataExcludingTerminator() const {
    return {reinterpret_cast<const Byte*>(firstStyle),
            reinterpret_cast<const Byte*>(terminatorStyle),
            unchecked};
  };

  STU_INLINE
  UInt lastStyleSizeInBytes() const {
    return sign_cast(reinterpret_cast<const Byte*>(terminatorStyle)
                     - reinterpret_cast<const Byte*>(&terminatorStyle->previous()));
  }
};

template <typename Bound>
STU_INLINE
TextFlags effectiveTextFlags(TextFlags flags, Range<Bound> range,
                             const TextStyleOverride& styleOverride,
                             Range<Bound> drawnRange, Range<Bound> overrideRange)
{
  if (!range.overlaps(overrideRange)) {
    return range.overlaps(drawnRange) ? flags : TextFlags{};
  }
  if (overrideRange.contains(range)) {
    flags &= styleOverride.flagsMask;
  }
  return flags |= styleOverride.flags;
}

STU_INLINE
TextFlags effectiveTextFlags(TextFlags flags, Range<TextFrameCompactIndex> range,
                            const TextStyleOverride& styleOverride)
{
  return effectiveTextFlags(flags, range,
                            styleOverride, styleOverride.drawnRange, styleOverride.overrideRange);
}

STU_INLINE
bool rectShadowOverlapsRect(Rect<CGFloat> r1, const TextStyle::ShadowInfo* __nullable r1Shadow,
                            Rect<CGFloat> r2)
{
  return r1Shadow && (r1 + r1Shadow->offset()).outset(r1Shadow->blurRadius).overlaps(r2);
}

} // namespace stu_label

