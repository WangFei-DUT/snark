// This file is part of snark, a generic and flexible library for robotics research
// Copyright (c) 2016 The University of Sydney
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the University of Sydney nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
// GRANTED BY THIS LICENSE.  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
// HOLDERS AND CONTRIBUTORS \"AS IS\" AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <boost/bind.hpp>
#include <comma/application/signal_flag.h>
#include <comma/application/verbose.h>
#include <comma/base/exception.h>
#include <snark/imaging/cv_mat/serialization.h>

#include "../attribute.h"
#include "../camera.h"
#include "../error.h"
#include "../frame.h"
#include "../system.h"

static const char* possible_fields = "t,rows,cols,type,size";
static const char* default_fields = "t,rows,cols,type";

static void bash_completion( unsigned const ac, char const * const * av )
{
    static const char* completion_options =
        " --help -h"
        " --verbose -v"
        " --version"
        " --list-cameras"
        " --list-attributes"
        " --set --set-and-exit"
        " --id --fields"
        " --header --no-header"
        ;

    std::cout << completion_options << std::endl;
    exit( 0 );
}

static void usage( bool verbose = false )
{
    std::cerr << std::endl;
    std::cerr << "Capture data from an Allied Vision GigE camera" << std::endl;
    std::cerr << "Output to stdout as serialized cv::Mat" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Usage: " << comma::verbose.app_name() << " [<options>]" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Options: " << std::endl;
    std::cerr << "    --help,-h:          show this help, --help --verbose for more help" << std::endl;
    std::cerr << "    --verbose,-v:       more output" << std::endl;
    std::cerr << "    --version:          output the library version" << std::endl;
    std::cerr << "    --list-cameras:     list all cameras and exit, --verbose for more detail" << std::endl;
    std::cerr << "    --list-attributes:  list camera attributes, --verbose for more detail" << std::endl;
    std::cerr << "    --set <attributes>: set camera attributes" << std::endl;
    std::cerr << "    --set-and-exit <attributes>: set attributes and exit" << std::endl;
    std::cerr << "    --id=<camera id>:   default: first available camera" << std::endl;
    std::cerr << "    --fields=<fields>:  header fields; default: " << default_fields << std::endl;
    std::cerr << "    --header:           output header only" << std::endl;
    std::cerr << "    --no-header:        output image data only" << std::endl;
    std::cerr << std::endl;
    std::cerr << "    Possible values for <fields> are: " << possible_fields << "." << std::endl;
    std::cerr << "    <attributes> are semicolon-separated name-value pairs." << std::endl;
    std::cerr << std::endl;
    std::cerr << "Examples:" << std::endl;
    std::cerr << "    " << comma::verbose.app_name() << " --id=\"02-2623A-07136\" --set \"ExposureAuto=Off;ExposureTimeAbs=60\"" << std::endl;
    std::cerr << std::endl;
    if( verbose )
    {
        std::cerr << "Camera id:" << std::endl;
        std::cerr << "    --list-cameras will provide two id's for each found camera: the camera id" << std::endl;
        std::cerr << "    and the serial number. Either can be used as the value for the --id option." << std::endl;
        std::cerr << std::endl;
        std::cerr << "Trouble-shooting:" << std::endl;
        std::cerr << "    Before running any vimba application you need to set GENICAM_GENTL64_PATH" << std::endl;
        std::cerr << "    to the path to the transport layer cti file. It is currently set to" << std::endl;
        std::cerr << "    " << getenv( "GENICAM_GENTL64_PATH" ) << "." << std::endl;
        std::cerr << std::endl;
        std::cerr << "Differences compared to gige-cat:" << std::endl;
        std::cerr << "    " << comma::verbose.app_name() << " does not support the integer id's supported by gige-cat." << std::endl;
        std::cerr << "    e.g. \"--id 180711\". Instead use one of the id's given by \"--list-cameras\"." << std::endl;
        std::cerr << std::endl;
        std::cerr << "    Many attributes have changed name or are configured in a different way." << std::endl;
        std::cerr << "    Use \"--list-attributes\" and \"--list-attributes --verbose\" to find the" << std::endl;
        std::cerr << "    appropriate new attribute." << std::endl;
        std::cerr << std::endl;
        std::cerr << "    " << comma::verbose.app_name() << " does not support image filters. Pipe to cv-cat instead." << std::endl;
        std::cerr << std::endl;
    }
    exit( 0 );
}

static comma::csv::format format_from_fields( const std::string& fields )
{
    std::vector< std::string > v = comma::split( fields, "," );
    comma::csv::format format;
    for( unsigned int i = 0; i < v.size(); ++i )
    {
        if( v[i] == "t" ) { format += "t"; }
        else { format += "ui"; }
    }
    return format;
}

// Word wrap input text to the given width, optionally inserting a prefix on each line
static std::string wrap( const std::string& text, size_t width = 80, const std::string& prefix = "")
{
    std::istringstream words( text );
    std::ostringstream wrapped;
    std::string word;

    size_t wrap_width = width - prefix.length();

    if( words >> word )
    {
        wrapped << word;
        size_t space_left = wrap_width - word.length();
        while( words >> word )
        {
            if( space_left < word.length() + 1 ) {
                wrapped << '\n' << prefix << word;
                space_left = wrap_width - word.length();
            } else {
                wrapped << ' ' << word;
                space_left -= word.length() + 1;
            }
        }
    }
    return wrapped.str();
}

static void print_attribute_entry( const std::string& label, const std::string& value )
{
    std::string prefix( label.length() + 2, ' ' );
    std::cout << label << ": " << wrap( value, 80, prefix ) << "\n";
}

static void output_frame( const snark::vimba::frame& frame
                        , snark::cv_mat::serialization& serialization )
{
    static VmbUint64_t last_frame_id = 0;

    // Take the timestamp immediately.
    //
    // The alternative is to use frame.timestamp() but that requires some
    // additional work to turn it into an actual timestamp and probably
    // isn't worth it without PTP.
    boost::posix_time::ptime timestamp( boost::posix_time::microsec_clock::universal_time() );

    if( frame.status() == VmbFrameStatusComplete )
    {
        if( last_frame_id != 0 )
        {
            VmbUint64_t missing_frames = frame.id() - last_frame_id - 1;
            if( missing_frames > 0 )
            {
                std::cerr << "Warning: " << missing_frames << " missing frame"
                          << ( missing_frames == 1 ? "" : "s" )
                          << " detected" << std::endl;
            }
        }
        last_frame_id = frame.id();

        snark::vimba::frame::pixel_format_desc fd = frame.format_desc();

        cv::Mat cv_mat( frame.height()
                      , frame.width() * fd.width_adjustment
                      , fd.type
                      , frame.image_buffer() );

        serialization.write( std::cout, std::make_pair( timestamp, cv_mat ));
    }
    else
    {
        std::cerr << "Warning: frame " << frame.id() << " status " << frame.status_as_string() << std::endl;
    }
}

static int run_cmd( const comma::command_line_options& options )
{
    snark::vimba::system system;                                   // Initialize the Vimba API

    if( options.exists( "--list-cameras" ))
    {
        AVT::VmbAPI::CameraPtrVector c = snark::vimba::system::cameras();
        if( comma::verbose ) { std::cout << "Cameras found: " << c.size() << std::endl; }
        for( AVT::VmbAPI::CameraPtrVector::const_iterator iter = c.begin(); iter != c.end(); ++iter )
        {
            snark::vimba::camera camera( *iter );
            snark::vimba::camera::name_values info = camera.info();
            if( comma::verbose )
            {
                std::cout << "\nCamera ID    : " << info["id"]
                          << "\nCamera Name  : " << info["name"]
                          << "\nModel Name   : " << info["model"]
                          << "\nSerial Number: " << info["serial_number"]
                          << "\nInterface ID : " << info["interface_id"]  << std::endl;
            }
            else
            {
                std::cout << "id=\"" << info["id"] << "\",name=\"" << info["name"]
                          << "\",serial=\"" << info["serial_number"] << "\"" << std::endl;
            }
        }
        return 0;
    }

    snark::vimba::camera camera( options.exists( "--id" )
                               ? snark::vimba::camera( options.value<std::string>( "--id" ))
                               : snark::vimba::camera( snark::vimba::system::open_first_camera()));

    if( options.exists( "--list-attributes" ))
    {
        std::vector< snark::vimba::attribute > attributes = camera.attributes();
        for( std::vector< snark::vimba::attribute >::const_iterator it = attributes.begin();
             it != attributes.end();
             ++it )
        {
            if( comma::verbose )
            {
                print_attribute_entry( "Name          ", it->name() );
                print_attribute_entry( "Type          ", it->type_as_string() );
                print_attribute_entry( "Value         ", it->value_as_string() );
                print_attribute_entry( "Description   ", it->description() );
                if( !it->allowed_values().empty() )
                    print_attribute_entry( "Allowed Values", it->allowed_values_as_string() );
                std::cout << std::endl;
            }
            else
            {
                std::cout << it->name() << "=" << it->value_as_string() << std::endl;
            }
        }
        return 0;
    }

    if( options.exists( "--set-and-exit" ))
    {
        camera.set_features( options.value<std::string>( "--set-and-exit" ));
        return 0;
    }

    if( options.exists( "--set" ))
    {
        camera.set_features( options.value<std::string>( "--set" ));
    }

    std::string        fields = options.value< std::string >( "--fields", default_fields );
    comma::csv::format format = format_from_fields( fields );
    bool               header_only = false;

    if( options.exists( "--no-header" ))
    {
        fields = "";
    }
    else
    {
        header_only = ( options.exists( "--header" ));
    }
    snark::cv_mat::serialization serialization( fields, format, header_only );

    camera.start_acquisition( boost::bind( &output_frame, _1, boost::ref( serialization )));

    comma::signal_flag is_shutdown;
    do {
        sleep( 1 );
    } while( !is_shutdown );

    camera.stop_acquisition();

    return 0;
}

#define QUOTED( arg ) #arg
#define STRINGIZED( arg ) QUOTED( arg )

int main( int argc, char** argv )
{
    if( !getenv( "GENICAM_GENTL64_PATH" ))
    {
        setenv( "GENICAM_GENTL64_PATH", STRINGIZED( VIMBA_GENICAM_GENTL64_PATH ), 0 );
    }

    int ret_code = 0;

    try
    {
        comma::command_line_options options( argc, argv, usage );
        if( options.exists( "--bash-completion" ) ) bash_completion( argc, argv );

        if( options.exists( "--version" ))
        {
            VmbVersionInfo_t version = snark::vimba::system::version();
            std::cout << "Vimba library version: " << version.major << "." << version.minor << "." << version.patch << std::endl;
            std::cout << "GENICAM_GENTL64_PATH=" << getenv( "GENICAM_GENTL64_PATH" ) << std::endl;
            return 0;
        }

        ret_code = run_cmd( options );
    }
    catch( std::exception& ex )
    {
        std::cerr << comma::verbose.app_name() << ": " << ex.what() << std::endl;
        ret_code = 1;
    }
    catch( ... )
    {
        std::cerr << comma::verbose.app_name() << ": unknown exception" << std::endl;
        ret_code = 1;
    }
    return ret_code;
}
