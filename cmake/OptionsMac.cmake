# FIXME: These should line up with versions in Version.xcconfig
set(PURC_MAC_VERSION 609.1.6)
set(MACOSX_FRAMEWORK_BUNDLE_VERSION 609.1.6+)

PURC_OPTION_BEGIN()
# Private options shared with other PurC ports. Add options here only if
# we need a value different from the default defined in PurCFeatures.cmake.
# FIXME: Many of these defaults differ from the XCode build.
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_OVERFLOW_SCROLLING_TOUCH PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_API_TESTS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_ASYNC_SCROLLING PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTENT_EXTENSIONS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS3_TEXT PRIVATE OFF)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_DRAG_SUPPORT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_DATE PRIVATE OFF)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_DATETIMELOCAL PRIVATE OFF)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_MONTH PRIVATE OFF)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_TIME PRIVATE OFF)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_LETTERPRESS PRIVATE OFF)

PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_3D_TRANSFORMS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLE_PAY PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_APPLICATION_MANIFEST PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_ATTACHMENT_ELEMENT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_AVF_CAPTIONS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CACHE_PARTITIONING PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CHANNEL_MESSAGING PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CONTENT_FILTERING PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_BOX_DECORATION_BREAK PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_COMPOSITING PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_CONIC_GRADIENTS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_PAINTING_API PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_SCROLL_SNAP PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_SELECTORS_LEVEL4 PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_TRAILING_WORD PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CSS_TYPED_OM PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_CURSOR_VISIBILITY PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_DARK_MODE_CSS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_DATACUE_VALUE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_DATALIST_ELEMENT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_ENCRYPTED_MEDIA PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_EXPERIMENTAL_FEATURES PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_FILTERS_LEVEL_2 PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_FTL_JIT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_FULLSCREEN_API PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_PICTURE_IN_PICTURE_API PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_GAMEPAD PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_GEOLOCATION PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INDEXED_DATABASE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INDEXED_DATABASE_IN_WORKERS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INPUT_TYPE_COLOR PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INSPECTOR_ALTERNATE_DISPATCHERS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INSPECTOR_TELEMETRY PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_INTERSECTION_OBSERVER PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_LAYOUT_FORMATTING_CONTEXT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_LEGACY_CSS_VENDOR_PREFIXES PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_LEGACY_CUSTOM_PROTOCOL_MANAGER PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_LEGACY_ENCRYPTED_MEDIA PRIVATE ON)
# FIXME: This should be enabled.
# PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_MAC_GESTURE_EVENTS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_MATHML PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_CONTROLS_SCRIPT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_SOURCE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEDIA_STREAM PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_MEMORY_SAMPLER PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_METER_ELEMENT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_MOUSE_CURSOR_SCALE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETWORK_CACHE_SPECULATIVE_REVALIDATION PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_NETWORK_CACHE_STALE_WHILE_REVALIDATE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_NOTIFICATIONS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_PAYMENT_REQUEST PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_PERIODIC_MEMORY_MONITOR PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_PDFKIT_PLUGIN PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_POINTER_LOCK PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_PUBLIC_SUFFIX_LIST PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_REMOTE_INSPECTOR PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESIZE_OBSERVER PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_LOAD_STATISTICS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_RESOURCE_USAGE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_RUBBER_BANDING PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_SANDBOX_EXTENSIONS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVER_PRECONNECT PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_CONTROLS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_SERVICE_WORKER PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_SHAREABLE_RESOURCE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_SPEECH_SYNTHESIS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_SVG_FONTS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_TELEPHONE_NUMBER_DETECTION PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_TEXT_AUTOSIZING PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_USERSELECT_ALL PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_USER_MESSAGE_HANDLERS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_VARIATION_FONTS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO_PRESENTATION_MODE PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_VIDEO_USES_ELEMENT_FULLSCREEN PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_MOUSE_INTERACTIONS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBDRIVER_KEYBOARD_INTERACTIONS PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGL PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGL2 PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEBGPU PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUDIO PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_AUTHN PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_CRYPTO PRIVATE ON)
# FIXME: This should be enabled.
# PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WEB_RTC PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_WIRELESS_PLAYBACK_TARGET PRIVATE ON)
PURC_OPTION_DEFAULT_PORT_VALUE(ENABLE_XSLT PRIVATE ON)

PURC_OPTION_END()

set(ENABLE_GRAPHICS_CONTEXT_GL ON)
set(ENABLE_PURC_LEGACY ON)
set(ENABLE_PURC ON)

set(JavaScriptCore_LIBRARY_TYPE SHARED)
set(xCore_LIBRARY_TYPE SHARED)
set(xCoreTestSupport_LIBRARY_TYPE SHARED)

find_package(ICU 60.2 REQUIRED COMPONENTS data i18n uc)
find_package(LibXml2 2.8.0 REQUIRED)
find_package(LibXslt 1.1.7 REQUIRED)
