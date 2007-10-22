// resolve.cc -- symbol resolution for gold

// Copyright 2006, 2007 Free Software Foundation, Inc.
// Written by Ian Lance Taylor <iant@google.com>.

// This file is part of gold.

// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
// MA 02110-1301, USA.

#include "gold.h"

#include "elfcpp.h"
#include "target.h"
#include "object.h"
#include "symtab.h"

namespace gold
{

// Symbol methods used in this file.

// Override the fields in Symbol.

template<int size, bool big_endian>
void
Symbol::override_base(const elfcpp::Sym<size, big_endian>& sym,
		      Object* object, const char* version)
{
  gold_assert(this->source_ == FROM_OBJECT);
  this->u_.from_object.object = object;
  if (version != NULL && this->version() != version)
    {
      gold_assert(this->version() == NULL);
      this->version_ = version;
    }
  // FIXME: Handle SHN_XINDEX.
  this->u_.from_object.shndx = sym.get_st_shndx();
  this->type_ = sym.get_st_type();
  this->binding_ = sym.get_st_bind();
  this->visibility_ = sym.get_st_visibility();
  this->nonvis_ = sym.get_st_nonvis();
  if (object->is_dynamic())
    this->in_dyn_ = true;
  else
    this->in_reg_ = true;
}

// Override the fields in Sized_symbol.

template<int size>
template<bool big_endian>
void
Sized_symbol<size>::override(const elfcpp::Sym<size, big_endian>& sym,
			     Object* object, const char* version)
{
  this->override_base(sym, object, version);
  this->value_ = sym.get_st_value();
  this->symsize_ = sym.get_st_size();
}

// Override TOSYM with symbol FROMSYM, defined in OBJECT, with version
// VERSION.  This handles all aliases of TOSYM.

template<int size, bool big_endian>
void
Symbol_table::override(Sized_symbol<size>* tosym,
		       const elfcpp::Sym<size, big_endian>& fromsym,
		       Object* object, const char* version)
{
  tosym->override(fromsym, object, version);
  if (tosym->has_alias())
    {
      Symbol* sym = this->weak_aliases_[tosym];
      gold_assert(sym != NULL);
      Sized_symbol<size>* ssym;
      ssym = this->get_sized_symbol SELECT_SIZE_NAME(size) (sym
							    SELECT_SIZE(size));
      do
	{
	  ssym->override(fromsym, object, version);
	  sym = this->weak_aliases_[ssym];
	  gold_assert(sym != NULL);
	  ssym = this->get_sized_symbol SELECT_SIZE_NAME(size) (
						sym SELECT_SIZE(size));
	}
      while (ssym != tosym);
    }
}

// The resolve functions build a little code for each symbol.
// Bit 0: 0 for global, 1 for weak.
// Bit 1: 0 for regular object, 1 for shared object
// Bits 2-3: 0 for normal, 1 for undefined, 2 for common
// This gives us values from 0 to 11.

static const int global_or_weak_shift = 0;
static const unsigned int global_flag = 0 << global_or_weak_shift;
static const unsigned int weak_flag = 1 << global_or_weak_shift;

static const int regular_or_dynamic_shift = 1;
static const unsigned int regular_flag = 0 << regular_or_dynamic_shift;
static const unsigned int dynamic_flag = 1 << regular_or_dynamic_shift;

static const int def_undef_or_common_shift = 2;
static const unsigned int def_flag = 0 << def_undef_or_common_shift;
static const unsigned int undef_flag = 1 << def_undef_or_common_shift;
static const unsigned int common_flag = 2 << def_undef_or_common_shift;

// Resolve a symbol.  This is called the second and subsequent times
// we see a symbol.  TO is the pre-existing symbol.  SYM is the new
// symbol, seen in OBJECT.  VERSION of the version of SYM.

template<int size, bool big_endian>
void
Symbol_table::resolve(Sized_symbol<size>* to,
		      const elfcpp::Sym<size, big_endian>& sym,
		      Object* object, const char* version)
{
  if (object->target()->has_resolve())
    {
      Sized_target<size, big_endian>* sized_target;
      sized_target = object->sized_target
                     SELECT_SIZE_ENDIAN_NAME(size, big_endian) (
                         SELECT_SIZE_ENDIAN_ONLY(size, big_endian));
      sized_target->resolve(to, sym, object, version);
      return;
    }

  if (!object->is_dynamic())
    {
      // Record that we've seen this symbol in a regular object.
      to->set_in_reg();
    }
  else
    {
      // Record that we've seen this symbol in a dynamic object.
      to->set_in_dyn();
    }

  unsigned int frombits;
  switch (sym.get_st_bind())
    {
    case elfcpp::STB_GLOBAL:
      frombits = global_flag;
      break;

    case elfcpp::STB_WEAK:
      frombits = weak_flag;
      break;

    case elfcpp::STB_LOCAL:
      gold_error(_("%s: invalid STB_LOCAL symbol %s in external symbols"),
		 object->name().c_str(), to->name());
      frombits = global_flag;
      break;

    default:
      gold_error(_("%s: unsupported symbol binding %d for symbol %s"),
		 object->name().c_str(),
		 static_cast<int>(sym.get_st_bind()), to->name());
      frombits = global_flag;
      break;
    }

  if (!object->is_dynamic())
    frombits |= regular_flag;
  else
    frombits |= dynamic_flag;

  switch (sym.get_st_shndx())
    {
    case elfcpp::SHN_UNDEF:
      frombits |= undef_flag;
      break;

    case elfcpp::SHN_COMMON:
      frombits |= common_flag;
      break;

    default:
      if (sym.get_st_type() == elfcpp::STT_COMMON)
	frombits |= common_flag;
      else
        frombits |= def_flag;
      break;
    }

  bool adjust_common_sizes;
  if (Symbol_table::should_override(to, frombits, object,
				    &adjust_common_sizes))
    {
      typename Sized_symbol<size>::Size_type tosize = to->symsize();

      this->override(to, sym, object, version);

      if (adjust_common_sizes && tosize > to->symsize())
        to->set_symsize(tosize);
    }
  else
    {
      if (adjust_common_sizes && sym.get_st_size() > to->symsize())
        to->set_symsize(sym.get_st_size());
    }
}

// Handle the core of symbol resolution.  This is called with the
// existing symbol, TO, and a bitflag describing the new symbol.  This
// returns true if we should override the existing symbol with the new
// one, and returns false otherwise.  It sets *ADJUST_COMMON_SIZES to
// true if we should set the symbol size to the maximum of the TO and
// FROM sizes.  It handles error conditions.

bool
Symbol_table::should_override(const Symbol* to, unsigned int frombits,
                              Object* object, bool* adjust_common_sizes)
{
  *adjust_common_sizes = false;

  unsigned int tobits;
  switch (to->binding())
    {
    case elfcpp::STB_GLOBAL:
      tobits = global_flag;
      break;

    case elfcpp::STB_WEAK:
      tobits = weak_flag;
      break;

    case elfcpp::STB_LOCAL:
      // We should only see externally visible symbols in the symbol
      // table.
      gold_unreachable();

    default:
      // Any target which wants to handle STB_LOOS, etc., needs to
      // define a resolve method.
      gold_unreachable();
    }

  if (to->source() == Symbol::FROM_OBJECT
      && to->object()->is_dynamic())
    tobits |= dynamic_flag;
  else
    tobits |= regular_flag;

  switch (to->shndx())
    {
    case elfcpp::SHN_UNDEF:
      tobits |= undef_flag;
      break;

    case elfcpp::SHN_COMMON:
      tobits |= common_flag;
      break;

    default:
      if (to->type() == elfcpp::STT_COMMON)
	tobits |= common_flag;
      else
        tobits |= def_flag;
      break;
    }

  // FIXME: Warn if either but not both of TO and SYM are STT_TLS.

  // We use a giant switch table for symbol resolution.  This code is
  // unwieldy, but: 1) it is efficient; 2) we definitely handle all
  // cases; 3) it is easy to change the handling of a particular case.
  // The alternative would be a series of conditionals, but it is easy
  // to get the ordering wrong.  This could also be done as a table,
  // but that is no easier to understand than this large switch
  // statement.

  // These are the values generated by the bit codes.
  enum
  {
    DEF =              global_flag | regular_flag | def_flag,
    WEAK_DEF =         weak_flag   | regular_flag | def_flag,
    DYN_DEF =          global_flag | dynamic_flag | def_flag,
    DYN_WEAK_DEF =     weak_flag   | dynamic_flag | def_flag,
    UNDEF =            global_flag | regular_flag | undef_flag,
    WEAK_UNDEF =       weak_flag   | regular_flag | undef_flag,
    DYN_UNDEF =        global_flag | dynamic_flag | undef_flag,
    DYN_WEAK_UNDEF =   weak_flag   | dynamic_flag | undef_flag,
    COMMON =           global_flag | regular_flag | common_flag,
    WEAK_COMMON =      weak_flag   | regular_flag | common_flag,
    DYN_COMMON =       global_flag | dynamic_flag | common_flag,
    DYN_WEAK_COMMON =  weak_flag   | dynamic_flag | common_flag
  };

  switch (tobits * 16 + frombits)
    {
    case DEF * 16 + DEF:
      // Two definitions of the same symbol.
      // FIXME: Do a better job of reporting locations.
      gold_error(_("%s: multiple definition of %s"),
		 object != NULL ? object->name().c_str() : _("command line"),
		 to->name());
      gold_error(_("%s: previous definition here"),
		 (to->source() == Symbol::FROM_OBJECT
		  ? to->object()->name().c_str()
		  : _("command line")));
      return false;

    case WEAK_DEF * 16 + DEF:
      // We've seen a weak definition, and now we see a strong
      // definition.  In the original SVR4 linker, this was treated as
      // a multiple definition error.  In the Solaris linker and the
      // GNU linker, a weak definition followed by a regular
      // definition causes the weak definition to be overridden.  We
      // are currently compatible with the GNU linker.  In the future
      // we should add a target specific option to change this.
      // FIXME.
      return true;

    case DYN_DEF * 16 + DEF:
    case DYN_WEAK_DEF * 16 + DEF:
      // We've seen a definition in a dynamic object, and now we see a
      // definition in a regular object.  The definition in the
      // regular object overrides the definition in the dynamic
      // object.
      return true;

    case UNDEF * 16 + DEF:
    case WEAK_UNDEF * 16 + DEF:
    case DYN_UNDEF * 16 + DEF:
    case DYN_WEAK_UNDEF * 16 + DEF:
      // We've seen an undefined reference, and now we see a
      // definition.  We use the definition.
      return true;

    case COMMON * 16 + DEF:
    case WEAK_COMMON * 16 + DEF:
    case DYN_COMMON * 16 + DEF:
    case DYN_WEAK_COMMON * 16 + DEF:
      // We've seen a common symbol and now we see a definition.  The
      // definition overrides.  FIXME: We should optionally issue, version a
      // warning.
      return true;

    case DEF * 16 + WEAK_DEF:
    case WEAK_DEF * 16 + WEAK_DEF:
      // We've seen a definition and now we see a weak definition.  We
      // ignore the new weak definition.
      return false;

    case DYN_DEF * 16 + WEAK_DEF:
    case DYN_WEAK_DEF * 16 + WEAK_DEF:
      // We've seen a dynamic definition and now we see a regular weak
      // definition.  The regular weak definition overrides.
      return true;

    case UNDEF * 16 + WEAK_DEF:
    case WEAK_UNDEF * 16 + WEAK_DEF:
    case DYN_UNDEF * 16 + WEAK_DEF:
    case DYN_WEAK_UNDEF * 16 + WEAK_DEF:
      // A weak definition of a currently undefined symbol.
      return true;

    case COMMON * 16 + WEAK_DEF:
    case WEAK_COMMON * 16 + WEAK_DEF:
      // A weak definition does not override a common definition.
      return false;

    case DYN_COMMON * 16 + WEAK_DEF:
    case DYN_WEAK_COMMON * 16 + WEAK_DEF:
      // A weak definition does override a definition in a dynamic
      // object.  FIXME: We should optionally issue a warning.
      return true;

    case DEF * 16 + DYN_DEF:
    case WEAK_DEF * 16 + DYN_DEF:
    case DYN_DEF * 16 + DYN_DEF:
    case DYN_WEAK_DEF * 16 + DYN_DEF:
      // Ignore a dynamic definition if we already have a definition.
      return false;

    case UNDEF * 16 + DYN_DEF:
    case WEAK_UNDEF * 16 + DYN_DEF:
    case DYN_UNDEF * 16 + DYN_DEF:
    case DYN_WEAK_UNDEF * 16 + DYN_DEF:
      // Use a dynamic definition if we have a reference.
      return true;

    case COMMON * 16 + DYN_DEF:
    case WEAK_COMMON * 16 + DYN_DEF:
    case DYN_COMMON * 16 + DYN_DEF:
    case DYN_WEAK_COMMON * 16 + DYN_DEF:
      // Ignore a dynamic definition if we already have a common
      // definition.
      return false;

    case DEF * 16 + DYN_WEAK_DEF:
    case WEAK_DEF * 16 + DYN_WEAK_DEF:
    case DYN_DEF * 16 + DYN_WEAK_DEF:
    case DYN_WEAK_DEF * 16 + DYN_WEAK_DEF:
      // Ignore a weak dynamic definition if we already have a
      // definition.
      return false;

    case UNDEF * 16 + DYN_WEAK_DEF:
    case WEAK_UNDEF * 16 + DYN_WEAK_DEF:
    case DYN_UNDEF * 16 + DYN_WEAK_DEF:
    case DYN_WEAK_UNDEF * 16 + DYN_WEAK_DEF:
      // Use a weak dynamic definition if we have a reference.
      return true;

    case COMMON * 16 + DYN_WEAK_DEF:
    case WEAK_COMMON * 16 + DYN_WEAK_DEF:
    case DYN_COMMON * 16 + DYN_WEAK_DEF:
    case DYN_WEAK_COMMON * 16 + DYN_WEAK_DEF:
      // Ignore a weak dynamic definition if we already have a common
      // definition.
      return false;

    case DEF * 16 + UNDEF:
    case WEAK_DEF * 16 + UNDEF:
    case DYN_DEF * 16 + UNDEF:
    case DYN_WEAK_DEF * 16 + UNDEF:
    case UNDEF * 16 + UNDEF:
      // A new undefined reference tells us nothing.
      return false;

    case WEAK_UNDEF * 16 + UNDEF:
    case DYN_UNDEF * 16 + UNDEF:
    case DYN_WEAK_UNDEF * 16 + UNDEF:
      // A strong undef overrides a dynamic or weak undef.
      return true;

    case COMMON * 16 + UNDEF:
    case WEAK_COMMON * 16 + UNDEF:
    case DYN_COMMON * 16 + UNDEF:
    case DYN_WEAK_COMMON * 16 + UNDEF:
      // A new undefined reference tells us nothing.
      return false;

    case DEF * 16 + WEAK_UNDEF:
    case WEAK_DEF * 16 + WEAK_UNDEF:
    case DYN_DEF * 16 + WEAK_UNDEF:
    case DYN_WEAK_DEF * 16 + WEAK_UNDEF:
    case UNDEF * 16 + WEAK_UNDEF:
    case WEAK_UNDEF * 16 + WEAK_UNDEF:
    case DYN_UNDEF * 16 + WEAK_UNDEF:
    case DYN_WEAK_UNDEF * 16 + WEAK_UNDEF:
    case COMMON * 16 + WEAK_UNDEF:
    case WEAK_COMMON * 16 + WEAK_UNDEF:
    case DYN_COMMON * 16 + WEAK_UNDEF:
    case DYN_WEAK_COMMON * 16 + WEAK_UNDEF:
      // A new weak undefined reference tells us nothing.
      return false;

    case DEF * 16 + DYN_UNDEF:
    case WEAK_DEF * 16 + DYN_UNDEF:
    case DYN_DEF * 16 + DYN_UNDEF:
    case DYN_WEAK_DEF * 16 + DYN_UNDEF:
    case UNDEF * 16 + DYN_UNDEF:
    case WEAK_UNDEF * 16 + DYN_UNDEF:
    case DYN_UNDEF * 16 + DYN_UNDEF:
    case DYN_WEAK_UNDEF * 16 + DYN_UNDEF:
    case COMMON * 16 + DYN_UNDEF:
    case WEAK_COMMON * 16 + DYN_UNDEF:
    case DYN_COMMON * 16 + DYN_UNDEF:
    case DYN_WEAK_COMMON * 16 + DYN_UNDEF:
      // A new dynamic undefined reference tells us nothing.
      return false;

    case DEF * 16 + DYN_WEAK_UNDEF:
    case WEAK_DEF * 16 + DYN_WEAK_UNDEF:
    case DYN_DEF * 16 + DYN_WEAK_UNDEF:
    case DYN_WEAK_DEF * 16 + DYN_WEAK_UNDEF:
    case UNDEF * 16 + DYN_WEAK_UNDEF:
    case WEAK_UNDEF * 16 + DYN_WEAK_UNDEF:
    case DYN_UNDEF * 16 + DYN_WEAK_UNDEF:
    case DYN_WEAK_UNDEF * 16 + DYN_WEAK_UNDEF:
    case COMMON * 16 + DYN_WEAK_UNDEF:
    case WEAK_COMMON * 16 + DYN_WEAK_UNDEF:
    case DYN_COMMON * 16 + DYN_WEAK_UNDEF:
    case DYN_WEAK_COMMON * 16 + DYN_WEAK_UNDEF:
      // A new weak dynamic undefined reference tells us nothing.
      return false;

    case DEF * 16 + COMMON:
      // A common symbol does not override a definition.
      return false;

    case WEAK_DEF * 16 + COMMON:
    case DYN_DEF * 16 + COMMON:
    case DYN_WEAK_DEF * 16 + COMMON:
      // A common symbol does override a weak definition or a dynamic
      // definition.
      return true;

    case UNDEF * 16 + COMMON:
    case WEAK_UNDEF * 16 + COMMON:
    case DYN_UNDEF * 16 + COMMON:
    case DYN_WEAK_UNDEF * 16 + COMMON:
      // A common symbol is a definition for a reference.
      return true;

    case COMMON * 16 + COMMON:
      // Set the size to the maximum.
      *adjust_common_sizes = true;
      return false;

    case WEAK_COMMON * 16 + COMMON:
      // I'm not sure just what a weak common symbol means, but
      // presumably it can be overridden by a regular common symbol.
      return true;

    case DYN_COMMON * 16 + COMMON:
    case DYN_WEAK_COMMON * 16 + COMMON:
      // Use the real common symbol, but adjust the size if necessary.
      *adjust_common_sizes = true;
      return true;

    case DEF * 16 + WEAK_COMMON:
    case WEAK_DEF * 16 + WEAK_COMMON:
    case DYN_DEF * 16 + WEAK_COMMON:
    case DYN_WEAK_DEF * 16 + WEAK_COMMON:
      // Whatever a weak common symbol is, it won't override a
      // definition.
      return false;

    case UNDEF * 16 + WEAK_COMMON:
    case WEAK_UNDEF * 16 + WEAK_COMMON:
    case DYN_UNDEF * 16 + WEAK_COMMON:
    case DYN_WEAK_UNDEF * 16 + WEAK_COMMON:
      // A weak common symbol is better than an undefined symbol.
      return true;

    case COMMON * 16 + WEAK_COMMON:
    case WEAK_COMMON * 16 + WEAK_COMMON:
    case DYN_COMMON * 16 + WEAK_COMMON:
    case DYN_WEAK_COMMON * 16 + WEAK_COMMON:
      // Ignore a weak common symbol in the presence of a real common
      // symbol.
      return false;

    case DEF * 16 + DYN_COMMON:
    case WEAK_DEF * 16 + DYN_COMMON:
    case DYN_DEF * 16 + DYN_COMMON:
    case DYN_WEAK_DEF * 16 + DYN_COMMON:
      // Ignore a dynamic common symbol in the presence of a
      // definition.
      return false;

    case UNDEF * 16 + DYN_COMMON:
    case WEAK_UNDEF * 16 + DYN_COMMON:
    case DYN_UNDEF * 16 + DYN_COMMON:
    case DYN_WEAK_UNDEF * 16 + DYN_COMMON:
      // A dynamic common symbol is a definition of sorts.
      return true;

    case COMMON * 16 + DYN_COMMON:
    case WEAK_COMMON * 16 + DYN_COMMON:
    case DYN_COMMON * 16 + DYN_COMMON:
    case DYN_WEAK_COMMON * 16 + DYN_COMMON:
      // Set the size to the maximum.
      *adjust_common_sizes = true;
      return false;

    case DEF * 16 + DYN_WEAK_COMMON:
    case WEAK_DEF * 16 + DYN_WEAK_COMMON:
    case DYN_DEF * 16 + DYN_WEAK_COMMON:
    case DYN_WEAK_DEF * 16 + DYN_WEAK_COMMON:
      // A common symbol is ignored in the face of a definition.
      return false;

    case UNDEF * 16 + DYN_WEAK_COMMON:
    case WEAK_UNDEF * 16 + DYN_WEAK_COMMON:
    case DYN_UNDEF * 16 + DYN_WEAK_COMMON:
    case DYN_WEAK_UNDEF * 16 + DYN_WEAK_COMMON:
      // I guess a weak common symbol is better than a definition.
      return true;

    case COMMON * 16 + DYN_WEAK_COMMON:
    case WEAK_COMMON * 16 + DYN_WEAK_COMMON:
    case DYN_COMMON * 16 + DYN_WEAK_COMMON:
    case DYN_WEAK_COMMON * 16 + DYN_WEAK_COMMON:
      // Set the size to the maximum.
      *adjust_common_sizes = true;
      return false;

    default:
      gold_unreachable();
    }
}

// A special case of should_override which is only called for a strong
// defined symbol from a regular object file.  This is used when
// defining special symbols.

bool
Symbol_table::should_override_with_special(const Symbol* to)
{
  bool adjust_common_sizes;
  unsigned int frombits = global_flag | regular_flag | def_flag;
  bool ret = Symbol_table::should_override(to, frombits, NULL,
					   &adjust_common_sizes);
  gold_assert(!adjust_common_sizes);
  return ret;
}

// Override symbol base with a special symbol.

void
Symbol::override_base_with_special(const Symbol* from)
{
  gold_assert(this->name_ == from->name_ || this->has_alias());

  this->source_ = from->source_;
  switch (from->source_)
    {
    case FROM_OBJECT:
      this->u_.from_object = from->u_.from_object;
      break;
    case IN_OUTPUT_DATA:
      this->u_.in_output_data = from->u_.in_output_data;
      break;
    case IN_OUTPUT_SEGMENT:
      this->u_.in_output_segment = from->u_.in_output_segment;
      break;
    case CONSTANT:
      break;
    default:
      gold_unreachable();
      break;
    }

  if (from->version_ != NULL && this->version_ != from->version_)
    {
      gold_assert(this->version_ == NULL);
      this->version_ = from->version_;
    }

  this->type_ = from->type_;
  this->binding_ = from->binding_;
  this->visibility_ = from->visibility_;
  this->nonvis_ = from->nonvis_;

  // Special symbols are always considered to be regular symbols.
  this->in_reg_ = true;

  if (from->needs_dynsym_entry_)
    this->needs_dynsym_entry_ = true;
  if (from->needs_dynsym_value_)
    this->needs_dynsym_value_ = true;

  // We shouldn't see these flags.  If we do, we need to handle them
  // somehow.
  gold_assert(!from->is_target_special_ || this->is_target_special_);
  gold_assert(!from->is_forwarder_);
  gold_assert(!from->has_got_offset_);
  gold_assert(!from->has_plt_offset_);
  gold_assert(!from->has_warning_);
  gold_assert(!from->is_copied_from_dynobj_);
}

// Override a symbol with a special symbol.

template<int size>
void
Sized_symbol<size>::override_with_special(const Sized_symbol<size>* from)
{
  this->override_base_with_special(from);
  this->value_ = from->value_;
  this->symsize_ = from->symsize_;
}

// Override TOSYM with the special symbol FROMSYM.  This handles all
// aliases of TOSYM.

template<int size>
void
Symbol_table::override_with_special(Sized_symbol<size>* tosym,
				    const Sized_symbol<size>* fromsym)
{
  tosym->override_with_special(fromsym);
  if (tosym->has_alias())
    {
      Symbol* sym = this->weak_aliases_[tosym];
      gold_assert(sym != NULL);
      Sized_symbol<size>* ssym;
      ssym = this->get_sized_symbol SELECT_SIZE_NAME(size) (sym
							    SELECT_SIZE(size));
      do
	{
	  ssym->override_with_special(fromsym);
	  sym = this->weak_aliases_[ssym];
	  gold_assert(sym != NULL);
	  ssym = this->get_sized_symbol SELECT_SIZE_NAME(size) (
						sym SELECT_SIZE(size));
	}
      while (ssym != tosym);
    }
}

// Instantiate the templates we need.  We could use the configure
// script to restrict this to only the ones needed for implemented
// targets.

#ifdef HAVE_TARGET_32_LITTLE
template
void
Symbol_table::resolve<32, false>(
    Sized_symbol<32>* to,
    const elfcpp::Sym<32, false>& sym,
    Object* object,
    const char* version);
#endif

#ifdef HAVE_TARGET_32_BIG
template
void
Symbol_table::resolve<32, true>(
    Sized_symbol<32>* to,
    const elfcpp::Sym<32, true>& sym,
    Object* object,
    const char* version);
#endif

#ifdef HAVE_TARGET_64_LITTLE
template
void
Symbol_table::resolve<64, false>(
    Sized_symbol<64>* to,
    const elfcpp::Sym<64, false>& sym,
    Object* object,
    const char* version);
#endif

#ifdef HAVE_TARGET_64_BIG
template
void
Symbol_table::resolve<64, true>(
    Sized_symbol<64>* to,
    const elfcpp::Sym<64, true>& sym,
    Object* object,
    const char* version);
#endif

#if defined(HAVE_TARGET_32_LITTLE) || defined(HAVE_TARGET_32_BIG)
template
void
Symbol_table::override_with_special<32>(Sized_symbol<32>*,
					const Sized_symbol<32>*);
#endif

#if defined(HAVE_TARGET_64_LITTLE) || defined(HAVE_TARGET_64_BIG)
template
void
Symbol_table::override_with_special<64>(Sized_symbol<64>*,
					const Sized_symbol<64>*);
#endif

} // End namespace gold.
