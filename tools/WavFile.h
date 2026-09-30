#pragma once

// =====================================================================================================================
// Musichien - WavFile
//
// Ecrit un WAV MONO 16 BITS sur le disque. Un outil de developpement, et rien d'autre : il vit dans tools/, il n'est
// embarque nulle part, et l'application ne le connait pas.
//
// C'est le format que tout lecteur audio ouvre sans discuter, et c'est exactement ce dont une oreille a besoin : seize
// bits suffisent tres largement pour juger une couleur ou une phrase, et un fichier brut ne demande aucune bibliotheque.
// =====================================================================================================================

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>

namespace musichien::tools
{

inline void writeWavFile( const std::filesystem::path & p_path,
                          std::span<const float> p_samples,
                          std::int32_t p_sampleRate )
{
    constexpr std::uint16_t CHANNEL_COUNT = 1;
    constexpr std::uint16_t BITS_PER_SAMPLE = 16;
    constexpr std::uint16_t BYTES_PER_SAMPLE = BITS_PER_SAMPLE / 8U;

    const auto sampleRate = static_cast<std::uint32_t>( p_sampleRate );
    const auto dataByteCount = static_cast<std::uint32_t>( p_samples.size() ) * BYTES_PER_SAMPLE;

    std::ofstream file{ p_path, std::ios::binary };

    if( !file )
    {
        std::cerr << "Musichien: cannot write " << p_path.string() << '\n';

        return;
    }

    const auto writeTag = [&file]( const char * p_tag ) {
        file.write( p_tag, 4 );
    };

    const auto writeUint32 = [&file]( std::uint32_t p_value ) {
        const std::array<char, 4> bytes{ static_cast<char>( p_value & 0xFFU ),
                                         static_cast<char>( ( p_value >> 8U ) & 0xFFU ),
                                         static_cast<char>( ( p_value >> 16U ) & 0xFFU ),
                                         static_cast<char>( ( p_value >> 24U ) & 0xFFU ) };

        file.write( bytes.data(), 4 );
    };

    const auto writeUint16 = [&file]( std::uint16_t p_value ) {
        // Le passage en 32 bits n'est pas cosmetique : un uint16_t est PROMU en int avant le decalage, et une operation
        // binaire sur un entier SIGNE est ce que clang-tidy signale (hicpp-signed-bitwise).
        const auto value = static_cast<std::uint32_t>( p_value );

        const std::array<char, 2> bytes{ static_cast<char>( value & 0xFFU ),
                                         static_cast<char>( ( value >> 8U ) & 0xFFU ) };

        file.write( bytes.data(), 2 );
    };

    writeTag( "RIFF" );
    writeUint32( 36U + dataByteCount );
    writeTag( "WAVE" );

    writeTag( "fmt " );
    writeUint32( 16U );
    writeUint16( 1U );    // PCM entier, sans compression
    writeUint16( CHANNEL_COUNT );
    writeUint32( sampleRate );
    writeUint32( sampleRate * static_cast<std::uint32_t>( CHANNEL_COUNT * BYTES_PER_SAMPLE ) );
    writeUint16( static_cast<std::uint16_t>( CHANNEL_COUNT * BYTES_PER_SAMPLE ) );
    writeUint16( BITS_PER_SAMPLE );

    writeTag( "data" );
    writeUint32( dataByteCount );

    for( const float sample : p_samples )
    {
        const auto clamped = static_cast<double>( std::clamp( sample, -1.0F, 1.0F ) );
        const auto value = static_cast<std::int16_t>( std::lround( clamped * 32767.0 ) );

        writeUint16( static_cast<std::uint16_t>( value ) );
    }
}

}    // namespace musichien::tools
