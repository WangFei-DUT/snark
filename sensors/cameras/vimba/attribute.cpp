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

#include <boost/lexical_cast.hpp>
#include <comma/application/verbose.h>
#include <comma/base/exception.h>
#include "error.h"
#include "attribute.h"

namespace snark { namespace vimba {

attribute::attribute( AVT::VmbAPI::FeaturePtr feature )
    : feature_( feature )
    , type_( VmbFeatureDataUnknown )
{
    VmbErrorType status = feature_->GetName( name_ );
    if( status != VmbErrorSuccess )
    {
        COMMA_THROW( comma::exception, error_msg( "GetName() failed", status ));
    }

    status = feature_->GetDataType( type_ );
    if( status != VmbErrorSuccess )
    {
        COMMA_THROW( comma::exception, error_msg( "GetDataType() failed", status ));
    }

    status = feature_->GetDescription( description_ );
    if( status != VmbErrorSuccess )
    {
        COMMA_THROW( comma::exception, error_msg( "GetDescription() failed", status ));
    }

    init_value();
    init_allowed_values();
}

void attribute::init_value()
{
    VmbErrorType status = VmbErrorSuccess;

    switch( type_ )
    {
        case VmbFeatureDataEnum:
            status = feature_->GetValue( value_as_string_ );
            if ( status == VmbErrorSuccess )
            {
                AVT::VmbAPI::EnumEntry enum_entry;
                status = feature_->GetEntry( enum_entry, value_as_string_.c_str() );
                if ( status == VmbErrorSuccess )
                {
                    // If you're debugging, you can use this enum_value to map back to
                    // the actual enum in VmbCommonTypes.h. Just look at the last byte.
                    status = enum_entry.GetValue( value_.enum_value );
                }
            }
            break;

        case VmbFeatureDataString:
            status = feature_->GetValue( value_as_string_ );
            break;

        case VmbFeatureDataBool:
            {
                status = feature_->GetValue( value_.bool_value );
                if ( status == VmbErrorSuccess )
                {
                    value_as_string_ = boost::lexical_cast< std::string >( value_.bool_value );
                }
            }
            break;

        case VmbFeatureDataFloat:
            status = feature_->GetValue( value_.float_value );
            if( status == VmbErrorSuccess)
            {
                value_as_string_ = boost::lexical_cast< std::string >( value_.float_value );
            }
            break;

        case VmbFeatureDataInt:
            {
                status = feature_->GetValue( value_.int_value );
                if( status == VmbErrorSuccess)
                {
                    value_as_string_ = boost::lexical_cast< std::string >( value_.int_value );
                }
            }
            break;

        case VmbFeatureDataUnknown:
            COMMA_THROW( comma::exception, "Can't determine value - type unknown" );
            break;

        case VmbFeatureDataCommand:
        default:
            value_as_string_ = "[None]";
            break;
    }

    if( status != VmbErrorSuccess )
    {
        COMMA_THROW( comma::exception, error_msg( "Could not get attribute value", status ));
    }
}

void attribute::init_allowed_values()
{
    VmbErrorType status = VmbErrorSuccess;

    switch( type_ )
    {
        case VmbFeatureDataEnum:
            {
                AVT::VmbAPI::EnumEntryVector entries;
                status = feature_->GetEntries( entries );
                if( status == VmbErrorSuccess)
                {
                    for( AVT::VmbAPI::EnumEntryVector::const_iterator it = entries.begin();
                         it != entries.end();
                         ++it )
                    {
                        std::string name;
                        status = it->GetName( name );
                        if( status != VmbErrorSuccess )
                            name = "<error - GetName()>";

                        VmbInt64_t value;
                        status = it->GetValue( value );
                        if( status != VmbErrorSuccess )
                        {
                            std::ostringstream msg;
                            msg << "Could not get value for " << name << " enum";
                            write_error( msg.str(), status );
                        }
                        else
                        {
                            bool is_available;
                            status = feature_->IsValueAvailable( value, is_available );
                            if( status != VmbErrorSuccess )
                            {
                                std::ostringstream msg;
                                msg << "Could not get availability status for " << name << " enum";
                                write_error( msg.str(), status );
                            }
                            else
                            {
                                if( is_available )
                                {
                                    allowed_values_.push_back( name );
                                }
                            }
                        }
                    }
                }
            }
            break;

        case VmbFeatureDataInt:
            {
                VmbInt64_t minimum, maximum;
                status = feature_->GetRange( minimum, maximum );
                if( status == VmbErrorSuccess)
                {
                    allowed_values_.push_back( boost::lexical_cast< std::string >( minimum ));
                    allowed_values_.push_back( boost::lexical_cast< std::string >( maximum ));
                }
            }
            break;

        case VmbFeatureDataFloat:
            {
                double minimum, maximum;
                status = feature_->GetRange( minimum, maximum );
                if( status == VmbErrorSuccess)
                {
                    allowed_values_.push_back( boost::lexical_cast< std::string >( minimum ));
                    allowed_values_.push_back( boost::lexical_cast< std::string >( maximum ));
                }
            }
            break;

        case VmbFeatureDataUnknown:
            COMMA_THROW( comma::exception, "Can't determine allowed values - type unknown" );
            break;

        default:
            break;
    }

    if( status != VmbErrorSuccess )
    {
        COMMA_THROW( comma::exception, error_msg( "Could not get allowed value", status ));
    }
}

const char* attribute::type_as_string() const
{
    switch( type_ )
    {
        case VmbFeatureDataUnknown: return "unknown";
        case VmbFeatureDataInt:     return "integer";
        case VmbFeatureDataFloat:   return "floating point";
        case VmbFeatureDataEnum:    return "enumeration";
        case VmbFeatureDataString:  return "string";
        case VmbFeatureDataBool:    return "boolean";
        case VmbFeatureDataCommand: return "command";
        case VmbFeatureDataRaw:     return "raw";
        case VmbFeatureDataNone:    return "no data";
    }
    return "";                          // Quiet gcc warning
}

std::string attribute::allowed_values_as_string() const
{
    std::ostringstream allowed_values_oss;
    switch( type_ )
    {
        case VmbFeatureDataFloat:
        case VmbFeatureDataInt:
            if( allowed_values_.size() == 2 )
            {
                allowed_values_oss << allowed_values_[0] << " to " << allowed_values_[1];
            }
            break;

        default:
            if( !allowed_values_.empty() )
            {
                std::copy( allowed_values_.begin(), allowed_values_.end() - 1
                         , std::ostream_iterator< std::string >( allowed_values_oss, ", " ));
                allowed_values_oss << allowed_values_.back();
            }
            break;
    }
    return allowed_values_oss.str();
}

void attribute::set( const std::string& value )
{
    VmbErrorType status = VmbErrorSuccess;

    if( comma::verbose )
    {
        std::cerr << "Setting \"" << name_ << "\" feature";
        if( !value.empty() )
        {
            std::cerr << " to " << type_as_string() << " value \"" << value << "\"";
        }
        std::cerr << std::endl;
    }

    switch( type_ )
    {
        case VmbFeatureDataUnknown:               // Unknown feature type
            COMMA_THROW( comma::exception, "unknown feature \"" << name_ << "\"" );
            break;
        case VmbFeatureDataInt:                   // 64 bit integer feature
            status = feature_->SetValue( boost::lexical_cast< VmbInt64_t >( value ));
            break;
        case VmbFeatureDataFloat:                 // 64 bit floating point feature
            status = feature_->SetValue( boost::lexical_cast< double >( value ));
            break;
        case VmbFeatureDataEnum:                  // Enumeration feature
        case VmbFeatureDataString:                // String feature
            status = feature_->SetValue( value.c_str() );
            break;
        case VmbFeatureDataBool:                  // Boolean feature
            status = feature_->SetValue( boost::lexical_cast< bool >( value ));
            break;
        case VmbFeatureDataCommand:               // Command feature
            status = feature_->RunCommand();
            if( status == VmbErrorSuccess )
            {
                bool is_command_done = false;
                do
                {
                    status = feature_->IsCommandDone( is_command_done );
                    if( status != VmbErrorSuccess ) break;
                } while( !is_command_done );
            }
            break;
        case VmbFeatureDataRaw:                   // Raw (direct register access) feature
            COMMA_THROW( comma::exception, "feature \"" << name_ << "\" is a direct register - setting this type is unsupported" );
            break;
        case VmbFeatureDataNone:                  // Feature with no data
            COMMA_THROW( comma::exception, "feature \"" << name_ << "\" has no data" );
            break;
    }

    if( status != VmbErrorSuccess )
    {
        std::string allowed_error_msg;
        if( !allowed_values_.empty() )
        {
            allowed_error_msg = "; allowed values: ";
            allowed_error_msg += allowed_values_as_string();
        }
        COMMA_THROW( comma::exception
                   , "failed to set feature \"" << name_ << " to " << value
                   << ", Error: " << status << ": " << error_code_to_string( status )
                   << allowed_error_msg );
    }
}

} } // namespace snark { namespace vimba {
