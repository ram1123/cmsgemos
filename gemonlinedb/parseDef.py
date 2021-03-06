#! /usr/bin/env python2

import xml.etree.ElementTree as ET

def _checkedJsonGet(json, fieldName, targetType):
    """
    Returns a value from a json dictionary only if it has the right type
    """
    assert(fieldName in json)
    assert(type(json[fieldName]) == targetType)
    return json[fieldName]

def _toCamelCase(snake_string, capitalizeFirstLetter=False):
    """
    Converts a name to camelCase.
    """
    # Adapted from https://stackoverflow.com/a/19053800
    components = snake_string.split('_')
    first = components[0]
    if capitalizeFirstLetter:
        first = first.title()
    else:
        first = first.lower()
    # We capitalize the first letter of each component except the first one
    # with the 'title' method and join them together.
    return first + ''.join(x.title() for x in components[1:])

class Field(object):
    """
    Represents a field in the database.
    """
    def __init__(self, json):
        self.name = _checkedJsonGet(json, 'name', unicode)

        if 'child names' in json:
            self.childNames = _checkedJsonGet(json, 'child names', list)
            for child in self.childNames:
                assert(type(child) == unicode)
        else:
            self.childNames = []

        if 'count' in json:
            self.count = _checkedJsonGet(json, 'count', int)
        else:
            self.count = 1

        if 'cpp name' in json:
            self._cppName = _checkedJsonGet(json, 'cpp name', unicode)
        else:
            self._cppName = _toCamelCase(self.name)

        if 'xsd only' in json:
            self.xsdOnly = _checkedJsonGet(json, 'xsd only', bool)
        else:
            self.xsdOnly = False

    def subElement(self, parent):
        """
        Creates subElements of parent defining this field in XSD syntax
        """
        if len(self.childNames) == 0:
            if self.count == 1:
                ET.SubElement(parent, 'xs:element', {
                    'name': self.name,
                    'type': 'xs:unsignedInt'
                })
            else:
                for i in range(self.count):
                    ET.SubElement(parent, 'xs:element', {
                        'name': self.name.format(i),
                        'type': 'xs:unsignedInt'
                    })
        else:
            # FIELD_NAME element
            element = ET.SubElement(parent, 'xs:element', {
                'name': '{}_NAME'.format(self.name),
            })
            simpleT = ET.SubElement(element, 'xs:simpleType')
            restriction = ET.SubElement(simpleT, 'xs:restriction', {
                'base': 'xs:string'
            })
            for childName in self.childNames:
                ET.SubElement(restriction, 'xs:enumeration', {
                    'value': childName
                })
            # FIELD_VALUE element
            ET.SubElement(parent, 'xs:element', {
                'name': '{}_VALUE'.format(self.name),
                'type': 'xs:unsignedInt'
            })

    def cppName(self, capitalizeFirstLetter = False):
        """
        Returns the name used to represent this C++ code
        """
        if capitalizeFirstLetter:
            return self._cppName[0].upper() + self._cppName[1:]
        else:
            return self._cppName

    def cppType(self):
        """
        Returns the C++ type to use for this field
        """
        if self.count == 1:
            return 'std::uint32_t'
        else:
            return 'std::array<std::uint32_t, {}>'''.format(self.count)

    def cppField(self):
        """
        Returns a declaration for this field in C++
        """
        if self.count == 1:
            return '''
                {} m_{};'''.format(self.cppType(), self.cppName())
        else:
            # Plural form
            return '''
                {} m_{}s;'''.format(self.cppType(), self.cppName())

    def cppGetters(self):
        """
        Returns the declaration of C++ get() functions for this field
        """
        if self.count == 1:
            return '''
                {2} get{1}() const {{ return m_{0}; }};'''.format(self.cppName(False),
                                                                  self.cppName(True),
                                                                  self.cppType())
        else:
            return '''
                const {2} &get{1}s() const {{ return m_{0}s; }};
                {2} &get{1}s() {{ return m_{0}s; }};
                std::uint32_t get{1}(std::size_t i) const {{ return m_{0}s.at(i); }};'''.format(
                    self.cppName(False),
                    self.cppName(True),
                    self.cppType())

    def cppSetters(self):
        """
        Returns the declaration of C++ set() functions for this field
        """
        if self.count == 1:
            return '''
                void set{1}(std::uint32_t value) {{ m_{0} = value; }};'''.format(
                    self.cppName(False),
                    self.cppName(True))
        else:
            return '''
                void set{1}s(const {2} &value) {{ m_{0}s = value; }};
                void set{1}(std::size_t i, std::uint32_t value) {{ m_{0}s.at(i) = value; }};'''.format(
                    self.cppName(False),
                    self.cppName(True),
                    self.cppType())

    def cppEq(self):
        """
        Returns the C++ code to add for this field in operator==
        """
        if self.count == 1:
            return '''
                    && get{0}() == other.get{0}()'''.format(self.cppName(True))
        else:
            return '''
                    && get{0}s() == other.get{0}s()'''.format(self.cppName(True))

    def cppReadRegisterData(self):
        """
        Returns the C++ code to add for this field in readRegisterData()
        """
        if self.count == 1:
            return '''
                set{1}(data.at("{0}"));'''.format(self.name, self.cppName(True))
        else:
            code = ''
            for i in range(self.count):
                code += '''
                set{1}({2}, data.at("{0}"));'''.format(self.name.format(i),
                                                       self.cppName(True),
                                                       i)
            return code

    def cppGetRegisterData(self):
        """
        Returns the C++ code to add for this field in getRegisterData()
        """
        if self.count == 1:
            return '''
                data["{0}"] = get{1}();'''.format(self.name, self.cppName(True))
        else:
            code = ''
            for i in range(self.count):
                code += '''
                data["{0}"] = get{1}({2});'''.format(self.name.format(i),
                                                     self.cppName(True),
                                                     i)
            return code

    def __repr__(self):
        return 'Field:{}'.format(self.name)

def _makeXml(config):
    extTableName = _checkedJsonGet(config, 'extension table name', unicode)
    kindOfPart = _checkedJsonGet(config, 'kind of part', unicode)
    fields = [ Field(f) for f in _checkedJsonGet(config, 'fields', list) ]

    schema = ET.Element('xs:schema', {
        'xmlns:xs': 'http://www.w3.org/2001/XMLSchema'
    })

    # Include common definitions
    redef = ET.SubElement(schema, 'xs:redefine', {'schemaLocation': 'common.xsd'})

    # Redefine the ExtensionTableName type to restrict its contents to extTableName
    simpleT = ET.SubElement(redef, 'xs:simpleType', {'name': 'ExtensionTableName'})
    restrict = ET.SubElement(simpleT, 'xs:restriction', {'base': 'ExtensionTableName'})
    ET.SubElement(restrict, 'xs:enumeration', {'value': extTableName})

    # Redefine the KindOfPart type to restrict its contents to kindOfPart
    simpleT = ET.SubElement(redef, 'xs:simpleType', {'name': 'KindOfPart'})
    restrict = ET.SubElement(simpleT, 'xs:restriction', {'base': 'KindOfPart'})
    ET.SubElement(restrict, 'xs:enumeration', {'value': kindOfPart})

    # Extend the Data type with elements for all fields
    cplxType = ET.SubElement(redef, 'xs:complexType', {'name': 'Data'})
    cplxContent = ET.SubElement(cplxType, 'xs:complexContent')
    extension = ET.SubElement(cplxContent, 'xs:extension', {'base': 'Data'})
    sequence = ET.SubElement(extension, 'xs:all')
    for f in fields:
        f.subElement(sequence)

    return ET.ElementTree(schema)

def _makeHeader(config, className):
    template = '''/*
 * THIS FILE WAS GENERATED
 *
 * Changes will be overwritten. Modify parseDef.py instead.
 */

#ifndef GEM_ONLINEDB_DETAIL_{baseNameCaps}GEN_H
#define GEM_ONLINEDB_DETAIL_{baseNameCaps}GEN_H

#include <cstdint>

#include "gem/onlinedb/ConfigurationTraits.h"
#include "gem/onlinedb/PartReference.h"
#include "gem/onlinedb/detail/RegisterData.h"

namespace gem {{
    namespace onlinedb {{
        namespace detail {{
            class {baseName}Gen
            {{
            private:{privateMembers}

            protected:
                explicit {baseName}Gen() {{ }}

            public:
                RegisterData getRegisterData() const;
                void readRegisterData(const RegisterData &data);

                bool operator== (const {baseName}Gen &other) const;
{publicMembers}
            }};
        }} /* namespace detail */

        template<>
        class ConfigurationTraits<detail::{baseName}Gen>
        {{
        public:
            static std::string extTableName() {{ return "{extTableName}"; }}
            static std::string typeName() {{ return "{typeName}"; }}
            static std::string kindOfPart() {{ return "{kindOfPart}"; }}
            using PartType = {partReference};
        }};
    }} /* namespace onlinedb */
}} /* namespace gem */

#endif // GEM_ONLINEDB_DETAIL_{baseNameCaps}GEN_H
'''
    fields = [ Field(f) for f in _checkedJsonGet(config, 'fields', list) ]

    privateMembers = ''
    for f in filter(lambda f: not f.xsdOnly, fields):
        privateMembers += f.cppField()

    publicMembers = ''
    for f in filter(lambda f: not f.xsdOnly, fields):
        publicMembers += f.cppGetters()
        publicMembers += "\n"
        publicMembers += f.cppSetters()

    extTableName = _checkedJsonGet(config, 'extension table name', unicode)
    typeName = _checkedJsonGet(config, 'type name', unicode)
    kindOfPart = _checkedJsonGet(config, 'kind of part', unicode)
    partReference = _checkedJsonGet(config, 'part reference', unicode)

    return template.format(baseName = className,
                           baseNameCaps = className.upper(),
                           privateMembers = privateMembers,
                           publicMembers = publicMembers,
                           extTableName = extTableName,
                           typeName = typeName,
                           kindOfPart = kindOfPart,
                           partReference = partReference)

def _makeCpp(config, className):
    template = '''/*
 * THIS FILE WAS GENERATED
 *
 * Changes will be overwritten. Modify parseDef.py instead.
 */

#include "gem/onlinedb/detail/{baseName}Gen.h"

namespace gem {{
    namespace onlinedb {{
        namespace detail {{
            bool {baseName}Gen::operator== (const {baseName}Gen &other) const
            {{
                return true{operatorEq};
            }}

            RegisterData {baseName}Gen::getRegisterData() const
            {{
                RegisterData data;
{getRegisterData}

                return data;
            }}

            void {baseName}Gen::readRegisterData(const RegisterData &data)
            {{{readRegisterData}
            }}
        }} /* namespace detail */
    }} /* namespace onlinedb */
}} /* namespace gem */
'''
    fields = [ Field(f) for f in _checkedJsonGet(config, 'fields', list) ]

    getRegisterData = ''
    for f in filter(lambda f: not f.xsdOnly, fields):
        getRegisterData += f.cppGetRegisterData()

    readRegisterData = ''
    for f in filter(lambda f: not f.xsdOnly, fields):
        readRegisterData += f.cppReadRegisterData()

    operatorEq = ''
    for f in filter(lambda f: not f.xsdOnly, fields):
        operatorEq += f.cppEq()

    return template.format(baseName = className,
                           getRegisterData = getRegisterData,
                           readRegisterData = readRegisterData,
                           operatorEq = operatorEq)

if __name__ == '__main__':
    import argparse
    import json
    import os

    parser = argparse.ArgumentParser(
        description='Creates C++ and XSD files from a JSON file')
    parser.add_argument('inputFile', metavar='input.json', type=file,
                        help='The input JSON file')
    args = parser.parse_args()

    config = json.load(args.inputFile)

    # Get the file name without extension nor base path (ie bar in /foo/bar.ext)
    baseFileName = os.path.splitext(os.path.basename(args.inputFile.name))[0]

    print('-- Generating: xml/schema/{}.xsd'.format(baseFileName))
    _makeXml(config).write('xml/schema/{}.xsd'.format(baseFileName), encoding='UTF-8')

    headerFileName = 'include/gem/onlinedb/detail/{}Gen.h'.format(baseFileName)
    with open(headerFileName, 'w') as f:
        print('-- Generating: {}'.format(f.name))
        f.write(_makeHeader(config, baseFileName))

    headerFileName = 'src/common/{}Gen.cc'.format(baseFileName)
    with open(headerFileName, 'w') as f:
        print('-- Generating: {}'.format(f.name))
        f.write(_makeCpp(config, baseFileName))
