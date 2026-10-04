#include "infrastructure/content/MarkdownCourse.h"

#include <charconv>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace musichien::infrastructure
{

namespace
{

// Whitespace, in the only sense a content file needs.
[[nodiscard]] bool isSpace( char p_character ) noexcept
{
    return ( p_character == ' ' ) || ( p_character == '\t' ) || ( p_character == '\r' ) || ( p_character == '\n' );
}

[[nodiscard]] std::string_view trim( std::string_view p_text ) noexcept
{
    std::size_t begin = 0;

    while( ( begin < p_text.size() ) && isSpace( p_text[begin] ) )
    {
        ++begin;
    }

    std::size_t end = p_text.size();

    while( ( end > begin ) && isSpace( p_text[end - 1] ) )
    {
        --end;
    }

    return p_text.substr( begin, end - begin );
}

// The lines, WITHOUT their line ending. The empty ones are kept, because an empty line is what ends a
// paragraph - and the paragraphs are what the page shows.
[[nodiscard]] std::vector<std::string_view> splitLines( std::string_view p_text )
{
    std::vector<std::string_view> lines;

    std::size_t position = 0;

    while( position <= p_text.size() )
    {
        const std::size_t lineEnd = p_text.find( '\n', position );

        if( lineEnd == std::string_view::npos )
        {
            lines.push_back( p_text.substr( position ) );

            break;
        }

        lines.push_back( p_text.substr( position, lineEnd - position ) );

        position = lineEnd + 1;
    }

    return lines;
}

// The fields of a directive, trimmed. ' | ' separates them, but one space on either side is a habit,
// not a rule: the separator is the bar itself.
[[nodiscard]] std::vector<std::string> splitFields( std::string_view p_line )
{
    std::vector<std::string> fields;

    std::size_t position = 0;

    while( true )
    {
        const std::size_t separator = p_line.find( '|', position );

        const std::string_view field = ( separator == std::string_view::npos ) ? p_line.substr( position )
                                                                               : p_line.substr( position, separator - position );

        fields.emplace_back( trim( field ) );

        if( separator == std::string_view::npos )
        {
            break;
        }

        position = separator + 1;
    }

    return fields;
}

// LA DISTANCE SE LIT DANS LA VALEUR, PAS DANS SON ORTHOGRAPHE.
//
// The contract writes 'demi_tons:7' because it reads well out loud. Refusing a plain '7' over that
// would teach the writer a rule that buys nothing: both are accepted.
[[nodiscard]] std::optional<std::int32_t> semitonesFromField( std::string_view p_field )
{
    std::string_view value = trim( p_field );

    const std::size_t colon = value.find( ':' );

    if( colon != std::string_view::npos )
    {
        value = trim( value.substr( colon + 1 ) );
    }

    if( value.empty() )
    {
        return std::nullopt;
    }

    std::int32_t semitones = 0;

    const auto [end, error] = std::from_chars( value.data(), value.data() + value.size(), semitones );

    if( ( error != std::errc{} ) || ( end != value.data() + value.size() ) )
    {
        return std::nullopt;
    }

    return semitones;
}

// The three words of the domain, as a lesson writes them.
//
// Anything else is read as ASCENDING rather than refused: an interval is heard from the bottom up
// first, and losing a card over a spelling mistake would cost more than it teaches.
[[nodiscard]] domain::IntervalDirection directionFromField( std::string_view p_field )
{
    const std::string_view word = trim( p_field );

    if( word == "descendant" )
    {
        return domain::IntervalDirection::Descending;
    }

    if( word == "harmonique" )
    {
        return domain::IntervalDirection::Harmonic;
    }

    return domain::IntervalDirection::Ascending;
}

// ---------------------------------------------------------------------------------------------------------------------
// UN PARAGRAPHE SE TERMINE SUR UNE LIGNE VIDE, une directive, ou la fin du fichier. Ce qui le ferme
// compte autant que lui : sans cela, deux paragraphes separes par une carte se colleraient en un seul.
// ---------------------------------------------------------------------------------------------------------------------
// Un bloc entre dans la lecon, ET dans la page ou il se trouve.
//
// Les deux listes sortent d'ici, et d'ici seulement : la page lit les sections, le « voir la note complete » lit la liste
// plate, et personne ne peut les faire diverger puisque c'est le meme geste qui les remplit.
void appendBlock( domain::Course & p_course, domain::CourseBlock p_block )
{
    // Le CHAPEAU : ce qui precede le premier titre. Il n'existe que s'il porte quelque chose - un cours qui commence
    // directement par un « ## » n'a pas de page vide devant lui.
    if( p_course.sections.empty() )
    {
        p_course.sections.push_back( domain::CourseSection{} );
    }

    p_course.sections.back().blocks.push_back( p_block );
    p_course.blocks.push_back( std::move( p_block ) );
}

void flushParagraph( std::string & p_paragraph, bool & p_inParagraph, domain::Course & p_course )
{
    if( !p_inParagraph )
    {
        return;
    }

    const std::string_view trimmed = trim( p_paragraph );

    if( !trimmed.empty() )
    {
        domain::CourseBlock block;
        block.kind = domain::CourseBlock::Kind::Text;
        block.markdown = std::string( trimmed );

        appendBlock( p_course, std::move( block ) );
    }

    p_paragraph.clear();
    p_inParagraph = false;
}

// ---------------------------------------------------------------------------------------------------------------------
// UNE DIRECTIVE, OU RIEN.
//
// Chaque refus dit POURQUOI sur stderr et rend la main : le cours continue avec une carte en moins.
// C'est le contrat, et c'est ce qui permet d'ecrire un cours en sachant qu'une barre mal placee ne
// coutera jamais l'application.
// ---------------------------------------------------------------------------------------------------------------------
[[nodiscard]] std::optional<domain::CourseBlock> readDirective( std::string_view p_line )
{
    const std::vector<std::string> fields = splitFields( trim( p_line.substr( 2 ) ) );

    if( fields.empty() )
    {
        return std::nullopt;
    }

    const std::string & keyword = fields.front();

    domain::CourseBlock block;

    if( keyword == "jeu" )
    {
        if( fields.size() < 3 )
        {
            std::cerr << "Musichien: a ':: jeu' card needs three fields: "
                         ":: jeu | demi_tons:7 | ascendant | ce que ca joue. It was skipped.\n";

            return std::nullopt;
        }

        const std::optional<std::int32_t> semitones = semitonesFromField( fields[1] );

        if( !semitones.has_value() )
        {
            std::cerr << "Musichien: a ':: jeu' card has no usable distance. It was skipped.\n";

            return std::nullopt;
        }

        block.kind = domain::CourseBlock::Kind::PlayInterval;
        block.semitones = *semitones;
        block.direction = directionFromField( fields[2] );

        if( fields.size() > 3 )
        {
            block.caption = fields[3];
        }

        return block;
    }

    if( keyword == "écoute" )
    {
        // LE CINQUIEME CHAMP N'EST PAS DECORATIF : c'est la SIGNALISATION, et le contrat en fait une
        // obligation. Un lien sans consigne d'ecoute est un lien qu'on ne sait pas ecouter - et le
        // refuser ici est ce qui empeche un cours de devenir une liste de liens.
        if( fields.size() < 5 )
        {
            std::cerr << "Musichien: a ':: écoute' card needs five fields: "
                         ":: écoute | youtube | url | titre | ce qu'il faut y entendre. It was skipped.\n";

            return std::nullopt;
        }

        block.kind = domain::CourseBlock::Kind::Listen;
        block.source = fields[1];
        block.url = fields[2];
        block.title = fields[3];
        block.listenFor = fields[4];

        return block;
    }

    if( keyword == "essai" )
    {
        if( fields.size() < 2 )
        {
            std::cerr << "Musichien: a ':: essai' card needs a distance. It was skipped.\n";

            return std::nullopt;
        }

        const std::optional<std::int32_t> semitones = semitonesFromField( fields[1] );

        if( !semitones.has_value() )
        {
            std::cerr << "Musichien: a ':: essai' card has no usable distance. It was skipped.\n";

            return std::nullopt;
        }

        block.kind = domain::CourseBlock::Kind::TryExercise;
        block.semitones = *semitones;

        return block;
    }

    // ":: chante | demi_tons:7 | ce qu'on demande de chanter"
    //
    // Meme forme que ':: essai', et la meme distance : ce que ce genre ajoute, ce n'est pas une donnee, c'est une PORTE -
    // celle qui ouvre l'outil de chant sur cet intervalle.
    if( keyword == "chante" )
    {
        if( fields.size() < 2 )
        {
            std::cerr << "Musichien: a ':: chante' card needs a distance. It was skipped.\n";

            return std::nullopt;
        }

        const std::optional<std::int32_t> semitones = semitonesFromField( fields[1] );

        if( !semitones.has_value() )
        {
            std::cerr << "Musichien: a ':: chante' card has no usable distance. It was skipped.\n";

            return std::nullopt;
        }

        block.kind = domain::CourseBlock::Kind::SingInterval;
        block.semitones = *semitones;

        if( fields.size() > 2 )
        {
            block.caption = fields[2];
        }

        return block;
    }

    if( keyword == "annexe" )
    {
        if( fields.size() < 2 )
        {
            std::cerr << "Musichien: a ':: annexe' card needs a name. It was skipped.\n";

            return std::nullopt;
        }

        block.kind = domain::CourseBlock::Kind::Annexe;
        block.annexeName = fields[1];

        return block;
    }

    std::cerr << "Musichien: a course directive is not one this game knows: '" << keyword
              << "'. It was skipped.\n";

    return std::nullopt;
}

// ---------------------------------------------------------------------------------------------------------------------
// L'EN-TETE : des lignes 'cle: valeur' entre deux tirets.
//
// Une cle INCONNUE est ignoree sans un mot. C'est ce qui permettra d'ajouter des champs plus tard sans
// casser les cours deja ecrits - et sans que personne ait a relire ceux d'avant.
// ---------------------------------------------------------------------------------------------------------------------
void readFrontMatterLine( std::string_view p_line, domain::Course & p_course )
{
    const std::string_view trimmed = trim( p_line );

    const std::size_t colon = trimmed.find( ':' );

    if( colon == std::string_view::npos )
    {
        return;
    }

    const std::string_view key = trim( trimmed.substr( 0, colon ) );
    const std::string_view value = trim( trimmed.substr( colon + 1 ) );

    if( key == "titre" )
    {
        p_course.title = std::string( value );
    }
    else if( key == "sous-titre" )
    {
        p_course.subtitle = std::string( value );
    }
    else if( key == "concepts" )
    {
        // DES DISTANCES, separees par des virgules : 'concepts: 7, 12'. Jamais des noms - c'est ce qui
        // fait qu'un cours peut nommer l'exercice qui lui correspond sans rien savoir du jeu.
        std::string_view remaining = value;

        while( !remaining.empty() )
        {
            const std::size_t comma = remaining.find( ',' );

            const std::string_view item = ( comma == std::string_view::npos ) ? remaining
                                                                              : remaining.substr( 0, comma );

            if( const std::optional<std::int32_t> semitones = semitonesFromField( item ); semitones.has_value() )
            {
                p_course.concepts.push_back( *semitones );
            }

            if( comma == std::string_view::npos )
            {
                break;
            }

            remaining = remaining.substr( comma + 1 );
        }
    }
    else if( ( key == "chapitre" ) || ( key == "ordre" ) )
    {
        if( const std::optional<std::int32_t> number = semitonesFromField( value ); number.has_value() )
        {
            if( key == "chapitre" )
            {
                p_course.chapter = static_cast<int>( *number );
            }
            else
            {
                p_course.order = static_cast<int>( *number );
            }
        }
    }
}

}    // namespace

std::optional<domain::Course> readCourse( std::string_view p_markdownText )
{
    const std::vector<std::string_view> lines = splitLines( p_markdownText );

    domain::Course course;

    std::size_t lineIndex = 0;

    // L'EN-TETE, QUAND IL Y EN A UN.
    if( ( !lines.empty() ) && ( trim( lines.front() ) == "---" ) )
    {
        ++lineIndex;

        for( ; lineIndex < lines.size(); ++lineIndex )
        {
            if( trim( lines[lineIndex] ) == "---" )
            {
                ++lineIndex;

                break;
            }

            readFrontMatterLine( lines[lineIndex], course );
        }
    }

    // LE CORPS, DANS L'ORDRE DU FICHIER. C'est l'ordre qui est la donnee : une carte se souvient de sa
    // place, et l'interface n'a rien a retrouver.
    std::string paragraph;
    bool inParagraph = false;

    for( ; lineIndex < lines.size(); ++lineIndex )
    {
        const std::string_view line = lines[lineIndex];
        const std::string_view trimmed = trim( line );

        if( trimmed.starts_with( "::" ) )
        {
            flushParagraph( paragraph, inParagraph, course );

            if( std::optional<domain::CourseBlock> block = readDirective( trimmed ); block.has_value() )
            {
                appendBlock( course, std::move( *block ) );
            }

            continue;
        }

        // UN TITRE DE NIVEAU 2 OUVRE UNE PAGE.
        //
        // C'est TOUTE la decoupe du cours, et elle vient de l'auteur du fichier : Roger veut plusieurs pages par chapitre,
        // et les « ## » sont exactement ces pages. Les ignorer pour tout aplatir, c'etait jeter une structure deja
        // ecrite - et c'est ce que faisait la premiere version.
        if( trimmed.starts_with( "## " ) )
        {
            flushParagraph( paragraph, inParagraph, course );

            domain::CourseSection section;
            section.title = std::string( trim( trimmed.substr( 3 ) ) );

            course.sections.push_back( std::move( section ) );

            continue;
        }

        if( trimmed.empty() )
        {
            flushParagraph( paragraph, inParagraph, course );

            continue;
        }

        // UN TITRE DE PREMIER NIVEAU SERT DE TITRE quand l'en-tete n'en a pas donne. Un cours qui a
        // oublie son en-tete reste un cours, et le perdre serait une faute de gout de la machine.
        // Seul le PREMIER compte : une fois le titre pris, la condition ne peut plus se verifier.
        if( course.title.empty() && trimmed.starts_with( "# " ) )
        {
            course.title = std::string( trim( trimmed.substr( 2 ) ) );
        }

        if( inParagraph )
        {
            paragraph.push_back( '\n' );
        }

        paragraph.append( line );
        inParagraph = true;
    }

    flushParagraph( paragraph, inParagraph, course );

    if( course.isEmpty() )
    {
        std::cerr << "Musichien: a course holds no usable content. It was skipped.\n";

        return std::nullopt;
    }

    return course;
}

}    // namespace musichien::infrastructure
