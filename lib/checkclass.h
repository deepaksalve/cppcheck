/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2010 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//---------------------------------------------------------------------------
#ifndef CheckClassH
#define CheckClassH
//---------------------------------------------------------------------------

#include "check.h"
#include "settings.h"
#include <map>

class Token;

/// @addtogroup Checks
/// @{


/** @brief %Check classes. Uninitialized member variables, non-conforming operators, missing virtual destructor, etc */
class CheckClass : public Check
{
public:
    /** @brief This constructor is used when registering the CheckClass */
    CheckClass() : Check(), hasSymbolDatabase(false)
    { }

    /** @brief This constructor is used when running checks. */
    CheckClass(const Tokenizer *tokenizer, const Settings *settings, ErrorLogger *errorLogger);

    ~CheckClass();

    /** @brief Run checks on the normal token list */
    void runChecks(const Tokenizer *tokenizer, const Settings *settings, ErrorLogger *errorLogger)
    {
        CheckClass checkClass(tokenizer, settings, errorLogger);

        // can't be a simplified check .. the 'sizeof' is used.
        checkClass.noMemset();
    }

    /** @brief Run checks on the simplified token list */
    void runSimplifiedChecks(const Tokenizer *tokenizer, const Settings *settings, ErrorLogger *errorLogger)
    {
        CheckClass checkClass(tokenizer, settings, errorLogger);

        // Coding style checks
        checkClass.constructors();
        checkClass.operatorEq();
        checkClass.privateFunctions();
        checkClass.operatorEqRetRefThis();
        checkClass.thisSubtraction();
        checkClass.operatorEqToSelf();

        checkClass.virtualDestructor();
        checkClass.checkConst();
    }


    /** @brief %Check that all class constructors are ok */
    void constructors();

    /** @brief %Check that all private functions are called */
    void privateFunctions();

    /**
     * @brief %Check that the memsets are valid.
     * The 'memset' function can do dangerous things if used wrong. If it
     * is used on STL containers for instance it will clear all its data
     * and then the STL container may leak memory or worse have an invalid state.
     * It can also overwrite the virtual table.
     * Important: The checking doesn't work on simplified tokens list.
     */
    void noMemset();

    /** @brief 'operator=' should return something. */
    void operatorEq();

    /** @brief 'operator=' should return reference to *this */
    void operatorEqRetRefThis();    // Warning upon no "return *this;"

    /** @brief 'operator=' should check for assignment to self */
    void operatorEqToSelf();    // Warning upon no check for assignment to self

    /** @brief The destructor in a base class should be virtual */
    void virtualDestructor();

    /** @brief warn for "this-x". The indented code may be "this->x"  */
    void thisSubtraction();

    /** @brief can member function be const? */
    void checkConst();

    /**
     * @brief Access control. This needs to be public, otherwise it
     * doesn't work to compile with Borland C++
     */
    enum AccessControl { Public, Protected, Private };

private:
    /**
     * @brief Create symbol database. For performance reasons, only call
     * it if it's needed.
     */
    void createSymbolDatabase();

    /**
     * @brief Prevent creating symbol database more than once.
     *
     * Initialize this flag to false in the constructors. If this flag
     * is true the createSymbolDatabase should just bail out. If it is
     * false the createSymbolDatabase will set it to true and create
     * the symbol database.
     */
    bool hasSymbolDatabase;

    /** @brief Information about a member variable. Used when checking for uninitialized variables */
    class Var
    {
    public:
        Var(const std::string &name_, bool init_ = false, bool priv_ = false, bool mutable_ = false, bool static_ = false, bool class_ = false, Var *next_ = 0)
            : name(name_),
              init(init_),
              priv(priv_),
              isMutable(mutable_),
              isStatic(static_),
              isClass(class_),
              next(next_)
        {
        }

        /** @brief name of variable */
        const std::string name;

        /** @brief has this variable been initialized? */
        bool        init;

        /** @brief is this variable declared in the private section? */
        bool        priv;

        /** @brief is this variable mutable? */
        bool        isMutable;

        /** @brief is this variable static? */
        bool        isStatic;

        /** @brief is this variable a class (or unknown type)? */
        bool        isClass;

        /** @brief next Var item */
        Var *next;

    private:
        Var& operator=(const Var&); // disallow assignments
    };

    struct Func
    {
        enum Type { Constructor, CopyConstructor, OperatorEqual, Destructor, Function };

        Func()
            : tokenDef(NULL),
              token(NULL),
              access(Public),
              hasBody(false),
              isInline(false),
              isConst(false),
              isVirtual(false),
              isStatic(false),
              isFriend(false),
              isOperator(false),
              type(Function)
        {
        }

        const Token *tokenDef; // function name token in class definition
        const Token *token;    // function name token in implementation
        AccessControl access;  // public/protected/private
        bool hasBody;          // has implementation
        bool isInline;         // implementation in class definition
        bool isConst;          // is const
        bool isVirtual;        // is virtual
        bool isStatic;         // is static
        bool isFriend;         // is friend
        bool isOperator;       // is operator
        Type type;             // constructor, destructor, ...
    };

    struct SpaceInfo
    {
        bool isNamespace;
        std::string className;
        const Token *classDef;   // class/struct/namespace token
        const Token *classStart; // '{' token
        const Token *classEnd;   // '}' token
        unsigned int numConstructors;
        std::list<Func> functionList;
        Var *varlist;
        std::vector<std::string> derivedFrom;
        SpaceInfo *nest;
        AccessControl access;
    };

    /** @brief Information about all namespaces/classes/structrues */
    std::multimap<std::string, SpaceInfo *> spaceInfoMMap;

    bool argsMatch(const Token *first, const Token *second, const std::string &path, unsigned int depth) const;

    /**
     * @brief parse a scope for a constructor or member function and set the "init" flags in the provided varlist
     * @param tok1 pointer to class declaration
     * @param ftok pointer to the function that should be checked
     * @param varlist variable list (the "init" flag will be set in these variables)
     * @param callstack the function doesn't look into recursive function calls.
     */
    void initializeVarList(const Token *tok1, const Token *ftok, Var *varlist, std::list<std::string> &callstack);

    /** @brief initialize a variable in the varlist */
    void initVar(Var *varlist, const std::string &varname);

    /**
     * @brief get varlist from a class definition
     * @param tok1 pointer to class definition
     */
    Var *getVarList(const Token *tok1);

    bool isMemberVar(const std::string &classname, const std::vector<std::string> &derivedFrom, const Var *varlist, const Token *tok);
    bool checkConstFunc(const std::string &classname, const std::vector<std::string> &derivedFrom, const Var *varlist, const Token *tok);

    /** @brief check if this function is virtual in the base classes */
    bool isVirtual(const std::vector<std::string> &derivedFrom, const Token *functionToken) const;

    // Reporting errors..
    void noConstructorError(const Token *tok, const std::string &classname, bool isStruct);
    void uninitVarError(const Token *tok, const std::string &classname, const std::string &varname);
    void operatorEqVarError(const Token *tok, const std::string &classname, const std::string &varname);
    void unusedPrivateFunctionError(const Token *tok, const std::string &classname, const std::string &funcname);
    void memsetClassError(const Token *tok, const std::string &memfunc);
    void memsetStructError(const Token *tok, const std::string &memfunc, const std::string &classname);
    void operatorEqReturnError(const Token *tok);
    void virtualDestructorError(const Token *tok, const std::string &Base, const std::string &Derived);
    void thisSubtractionError(const Token *tok);
    void operatorEqRetRefThisError(const Token *tok);
    void operatorEqToSelfError(const Token *tok);

    void checkConstError(const Token *tok, const std::string &classname, const std::string &funcname);
    void checkConstError2(const Token *tok1, const Token *tok2, const std::string &classname, const std::string &funcname);

    void getErrorMessages()
    {
        noConstructorError(0, "classname", false);
        uninitVarError(0, "classname", "varname");
        operatorEqVarError(0, "classname", "");
        unusedPrivateFunctionError(0, "classname", "funcname");
        memsetClassError(0, "memfunc");
        memsetStructError(0, "memfunc", "classname");
        operatorEqReturnError(0);
        virtualDestructorError(0, "Base", "Derived");
        thisSubtractionError(0);
        operatorEqRetRefThisError(0);
        operatorEqToSelfError(0);
        checkConstError(0, "class", "function");
    }

    std::string name() const
    {
        return "Class";
    }

    std::string classInfo() const
    {
        return "Check the code for each class.\n"
               "* Missing constructors\n"
               "* Are all variables initialized by the constructors?\n"
               "* [[CheckMemset|Warn if memset, memcpy etc are used on a class]]\n"
               "* If it's a base class, check that the destructor is virtual\n"
               "* Are there unused private functions\n"
               "* 'operator=' should return reference to self\n"
               "* 'operator=' should check for assignment to self\n"
               "* Constness for member functions\n";
    }
};
/// @}
//---------------------------------------------------------------------------
#endif

