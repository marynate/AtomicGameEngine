//
// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
// LICENSE: Atomic Game Engine Editor and Tools EULA
// Please see LICENSE_ATOMIC_EDITOR_AND_TOOLS.md in repository root for
// license information: https://github.com/AtomicGameEngine/AtomicGameEngine
//

#include <Atomic/Atomic.h>
#include <Atomic/IO/Log.h>
#include <Atomic/Core/ProcessUtils.h>
#include <Atomic/Resource/ResourceCache.h>

#include "JSBind.h"
#include "JSBPackage.h"
#include "JSBModule.h"
#include "JSBFunction.h"
#include "JSBTypeScript.h"

namespace ToolCore
{

String JSBTypeScript::GetScriptType(JSBFunctionType* ftype)
{
    String scriptType = "number";

    if (ftype->type_->asPrimitiveType())
    {
        JSBPrimitiveType* ptype = ftype->type_->asPrimitiveType();
        if (ptype->kind_ == JSBPrimitiveType::Bool)
            scriptType = "boolean";

    }

    if (ftype->type_->asStringHashType() || ftype->type_->asStringType())
        scriptType = "string";

    if (ftype->type_->asEnumType())
    {
        JSBEnum* jenum = ftype->type_->asEnumType()->enum_;

        scriptType = jenum->GetName();

        if (jenum->GetPackage()->GetName() != package_->GetName())
        {

            scriptType =jenum->GetPackage()->GetName() + "." + scriptType;

        }

    }

    if (ftype->type_->asClassType())
    {
        JSBClass* klass = ftype->type_->asClassType()->class_;

        scriptType = klass->GetName();

        if (klass->GetPackage()->GetName() != package_->GetName())
        {

            scriptType = klass->GetPackage()->GetName() + "." + klass->GetName();

        }

    }

    if (ftype->type_->asVectorType())
    {
        scriptType = "string[]";
    }

    return scriptType;

}

void JSBTypeScript::Begin()
{
    source_ += "//Atomic TypeScript Definitions\n\n\n";

    if (package_->GetName() != "Atomic")
    {
        source_ += "/// <reference path=\"Atomic.d.ts\" />\n\n";
    }

    source_ += "declare module "+ package_->GetName() + " {\n\n";
}

void JSBTypeScript::End()
{
    source_ += "\n}\n";
}

void JSBTypeScript::ExportFunction(JSBFunction* function)
{
    if (function->Skip())
        return;

    String scriptName = "constructor";

    //constructor(width?: number, height?: number, options?: PixiRendererOptions);

    if (!function->IsConstructor())
    {
        scriptName =  function->GetName();
        scriptName[0] = tolower(scriptName[0]);
    }

    if (function->GetDocString().Length())
        source_ += "      //" + function->GetDocString() + "\n";

    source_ += "      ";

    if (function->IsStatic())
        source_ += "static ";

    source_ += scriptName + "(";

    Vector<JSBFunctionType*>& parameters = function->GetParameters();

    for (unsigned i = 0; i < parameters.Size(); i++)
    {
        JSBFunctionType* ftype = parameters.At(i);

        String scriptType = GetScriptType(ftype);

        if (scriptType == "Context" || scriptType == "Atomic.Context")
            continue;

        String name = ftype->name_;

        // TS doesn't like arguments named arguments
        if (name == "arguments")
            name = "args";

        source_ += name;

        if (ftype->initializer_.Length())
            source_ += "?";

        source_ += ": " + scriptType;

        if (i < parameters.Size() - 1)
            source_ += ", ";
    }

    if (function->IsConstructor())
        source_ += ");\n";
    else
    {
        if (!function->GetReturnType())
            source_ += "): void;\n";
        else
            source_ += "): " + GetScriptType(function->GetReturnType()) + ";\n";

    }



}

void JSBTypeScript::ExportModuleClasses(JSBModule* module)
{
    Vector<SharedPtr<JSBClass>> classes = module->GetClasses();

    if (!classes.Size())
        return;

    source_ += "\n";

    for (unsigned i = 0; i < classes.Size(); i++)
    {
        JSBClass* klass = classes.At(i);

        source_ += "   export class " + klass->GetName();

        JSBClass* base = klass->GetBaseClass();

        if (base)
        {
            if (klass->GetPackage() != base->GetPackage())
            {
                source_ += " extends " + base->GetPackage()->GetName() + "." + base->GetName();
            }
            else
            {
                source_ += " extends " + base->GetName();
            }

        }

        source_ += " {\n\n";

        Vector<String> propertyNames;

        klass->GetPropertyNames(propertyNames);

        for (unsigned j = 0; j < propertyNames.Size(); j++)
        {

            JSBProperty* prop = klass->GetProperty(propertyNames[j]);

            JSBFunctionType* ftype = NULL;

            if (prop->getter_ && !prop->getter_->Skip())
            {
                ftype = prop->getter_->GetReturnType();
            }
            else if (prop->setter_ && !prop->setter_->Skip())
                ftype = prop->setter_->GetParameters()[0];

            if (!ftype)
                continue;

            String scriptType = GetScriptType(ftype);
            String scriptName = prop->GetCasePropertyName();

            source_ += "      " + scriptName + ": " + scriptType + ";\n";

        }

        if (propertyNames.Size())
            source_ += "\n";

        JSBFunction* constructor = klass->GetConstructor();
        if (constructor)
        {
            ExportFunction(constructor);
            source_ += "\n";
        }

        PODVector<JSBFunction*>& functions = klass->GetFunctions();

        for (unsigned j = 0; j < functions.Size(); j++)
        {

            JSBFunction* func = functions[j];

            if (func->IsConstructor() || func->IsDestructor() || func->Skip())
                continue;

            ExportFunction(func);


        }

        for (unsigned j = 0; j < klass->GetNumTypeScriptDecl(); j++)
        {
            source_ += "      " + klass->GetTypeScriptDecl(j) + "\n";
        }



        source_ += "\n   }\n\n";

    }

    source_ += "\n";

}


void JSBTypeScript::ExportModuleConstants(JSBModule* module)
{
    const Vector<String>& constants = module->GetConstants().Keys();

    if (!constants.Size())
        return;

    source_ += "\n";

    for (unsigned i = 0; i < constants.Size(); i++)
    {
        const String& cname = constants.At(i);

        source_ += "   export var " + cname + ": number;\n";
    }

    source_ += "\n";

}

void JSBTypeScript::ExportModuleEnums(JSBModule* module)
{

    Vector<SharedPtr<JSBEnum>> enums = module->GetEnums();

    for (unsigned i = 0; i <enums.Size(); i++)
    {
        JSBEnum* _enum = enums[i];

        // can't use a TS enum, so use a type alias

        source_ += "\n   // enum " + _enum->GetName() + "\n";
        source_ += "   export type " + _enum->GetName() + " = number;\n";

        HashMap<String, String>& values = _enum->GetValues();

        HashMap<String, String>::ConstIterator itr = values.Begin();

        while (itr != values.End())
        {
            String name = (*itr).first_;

            source_ += "   export var " + name + ": " +  _enum->GetName() + ";\n";

            itr++;
        }

        source_ += "\n";

    }

}

void JSBTypeScript::WriteToFile(const String &path)
{
    File file(package_->GetContext());
    file.Open(path, FILE_WRITE);

    file.Write(source_.CString(), source_.Length());

    file.Close();

}

void JSBTypeScript::Emit(JSBPackage* package, const String& path)
{
    package_ = package;

    Vector<SharedPtr<JSBModule>>& modules = package->GetModules();

    Begin();

    for (unsigned i = 0; i < modules.Size(); i++)
    {
        ExportModuleEnums(modules[i]);
    }

    for (unsigned i = 0; i < modules.Size(); i++)
    {
        ExportModuleConstants(modules[i]);
    }

    for (unsigned i = 0; i < modules.Size(); i++)
    {
        source_ += "\n//----------------------------------------------------\n";
        source_ += "// MODULE: " + modules[i]->GetName() + "\n";
        source_ += "//----------------------------------------------------\n\n";
        ExportModuleClasses(modules[i]);
    }


    End();
    WriteToFile(path);

}

}
