#include "ui/IntervalDescription.h"

#include <QString>

namespace musichien::ui
{

QVariantMap describeInterval( const domain::Interval & p_interval )
{
    return QVariantMap{
      { QStringLiteral( "identifier" ), QString::fromStdString( p_interval.identifier() ) },
      { QStringLiteral( "name" ), QString::fromStdString( p_interval.name() ) },
      { QStringLiteral( "semitones" ), p_interval.semitones() },
      { QStringLiteral( "intervalClass" ), p_interval.intervalClass() },
      { QStringLiteral( "octaveSpan" ), p_interval.octaveSpan() },
      { QStringLiteral( "number" ), p_interval.number() },
      { QStringLiteral( "isCompound" ), p_interval.isCompound() },
    };
}

}    // namespace musichien::ui
