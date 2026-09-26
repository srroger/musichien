# =====================================================================================================================
# Musichien - source tree
#
# The modules are ordered by dependency. The arrow always points towards the domain:
#
#     application  ->  domain
#     ui           ->  application  ->  domain
#     infrastructure ->  domain
#
# 'domain' depends on nothing but the C++ standard library. That is what makes it testable without a
# graphical interface and without a device. See docs/ARCHITECTURE.md.
# =====================================================================================================================

add_subdirectory(domain)
add_subdirectory(ui)
add_subdirectory(infrastructure)
add_subdirectory(application)

