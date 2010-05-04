/***************************************************************************
			prognode.cpp  -  GDL's AST is made of ProgNodes
			-------------------
begin                : July 22 2002
copyright            : (C) 2002 by Marc Schellens
email                : m_schellens@users.sf.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "includefirst.hpp"

#include <memory>

#include "prognodeexpr.hpp"

#include "dinterpreter.hpp"

#include "objects.hpp"

using namespace std;

/////////////////////////////////////////////////////////////////////////////
// c-tor d-tor
/////////////////////////////////////////////////////////////////////////////

// tanslation RefDNode -> ProgNode
ProgNode::ProgNode( const RefDNode& refNode):
	keepRight( false),
	keepDown( false),
	breakTarget( NULL),
  ttype( refNode->getType()),
  text( refNode->getText()),
  down( NULL), 
  right( NULL),
  lineNumber( refNode->getLine()),
  cData( refNode->StealCData()),
  libPro( refNode->libPro),
  libFun( refNode->libFun),
  var( refNode->var),
  arrIxList( refNode->StealArrIxList()),
//   arrIxList( refNode->CloneArrIxList()),
  labelStart( refNode->labelStart),
  labelEnd( refNode->labelEnd)
{
  initInt = refNode->initInt;
}



ProgNode::~ProgNode()
{
  // delete cData in case this node is a constant
  if( (getType() == GDLTokenTypes::CONSTANT))
     {
      delete cData;
     }
  if( (getType() == GDLTokenTypes::ARRAYIX))
    {
      delete arrIxList;
    }
  if( !keepDown) delete down;
  if( !keepRight) delete right;
}



ProgNodeP ProgNode::GetNULLProgNodeP()
  {
	return interpreter->GetNULLProgNodeP();
  }


void FORNode::KeepRight( ProgNodeP r)
{
	throw GDLException( "Internal error: FORNode::KeepRight() called. Please report.");
}
void FOR_STEPNode::KeepRight( ProgNodeP r)
{
	throw GDLException( "Internal error: FOR_STEPNode::KeepRight() called. Please report.");
}
void FOREACHNode::KeepRight( ProgNodeP r)
{
	throw GDLException( "Internal error: FOREACHNode::KeepRight() called. Please report.");
}

bool ProgNode::ConstantNode()
  {
    if( this->getType() == GDLTokenTypes::SYSVAR)
      {
	// note: this works, because system variables are never 
	//       passed by reference
       SizeT rdOnlySize = sysVarRdOnlyList.size();
         for( SizeT i=0; i<rdOnlySize; ++i)
                  if( sysVarRdOnlyList[ i] == this->var)
					return true;
      }

    return this->getType() == GDLTokenTypes::CONSTANT;
  }

/////////////////////////////////////////////////////////
// Eval 
/////////////////////////////////////////////////////////
BaseGDL* ARRAYDEFNode::Eval()
{
  // GDLInterpreter::
  DType  cType=UNDEF; // conversion type
  SizeT maxRank=0;
  ExprListT            exprList;
  BaseGDL*           cTypeData;
		
  //ProgNodeP __t174 = this;
  //ProgNodeP a =  this;
  //match(antlr::RefAST(_t),ARRAYDEF);
  ProgNodeP _t =  this->getFirstChild();
  while(  _t != NULL) {

    BaseGDL* e=_t->Eval();//expr(_t);
    _t = _t->getNextSibling();
    //WRONG    _t = ProgNode::interpreter->_retTree;
			
    // add first (this way it will get cleaned up anyway)
    exprList.push_back(e);
			
    DType ty=e->Type();
    if( ty == UNDEF)
      {
	throw GDLException( _t, "Variable is undefined: "+
			    ProgNode::interpreter->Name(e),true,false);
      }
    if( cType == UNDEF) 
      {
	cType=ty;
	cTypeData=e;
      }
    else 
      { 
	if( cType != ty) 
	  {
	    if( DTypeOrder[ty] > 100 || DTypeOrder[cType] > 100) // struct, ptr, object
	      {
		throw 
		  GDLException( _t, e->TypeStr()+
				" is not allowed in this context.",true,false);
	      }
			
	    // update order if larger type (or types are equal)
	    if( DTypeOrder[ty] >= DTypeOrder[cType]) 
	      {
		cType=ty;
		cTypeData=e;
	      }
	  }
	if( ty == STRUCT)
	  {
	    // check for struct compatibility
	    DStructDesc* newS=
	      static_cast<DStructGDL*>(e)->Desc();
	    DStructDesc* oldS=
	      static_cast<DStructGDL*>(cTypeData)->Desc();
			
	    // *** here (*newS) != (*oldS) must be set when
	    // unnamed structs not in struct list anymore
	    // WRONG! This speeds up things for named structs
	    // unnamed structs all have their own desc
	    // and thus the next is always true for them
	    if( newS != oldS)
	      {
			
		if( (*newS) != (*oldS))
		  throw GDLException( _t, 
				      "Conflicting data structures: "+
				      ProgNode::interpreter->Name(cTypeData)+", "+ProgNode::interpreter->Name(e),true,false);
	      }
	  }
      }
			
    // memorize maximum Rank
    SizeT rank=e->Rank();
    if( rank > maxRank) maxRank=rank;
  }
  _t = this->getNextSibling();
	
  BaseGDL* res=cTypeData->CatArray(exprList,this->arrayDepth,maxRank);
	
  ProgNode::interpreter->_retTree = _t;
  return res;
}



BaseGDL* STRUCNode::Eval()
{
  // don't forget the struct in extrat.cpp if you change something here
  // "$" as first char in the name is necessary 
  // as it defines unnnamed structs (see dstructdesc.hpp)
  DStructDesc*   nStructDesc = new DStructDesc( "$truct");
	
  // instance takes care of nStructDesc since it is unnamed
  //     DStructGDL* instance = new DStructGDL( nStructDesc, dimension(1));
  DStructGDL* instance = new DStructGDL( nStructDesc);
  auto_ptr<DStructGDL> instance_guard(instance);
	
  ProgNodeP rTree = this->getNextSibling();
  // 	match(antlr::RefAST(_t),STRUC);
  ProgNodeP _t = this->getFirstChild();
  for (; _t != NULL;) {
    ProgNodeP si = _t;
    // 			match(antlr::RefAST(_t),IDENTIFIER);
    _t = _t->getNextSibling();
    BaseGDL* e=_t->Eval();//expr(_t);
    _t = _t->getNextSibling();
    //WRONG    _t = ProgNode::interpreter->_retTree;
			
    // also adds to descriptor, grabs
    instance->NewTag( si->getText(), e); 
  }

  instance_guard.release();
  BaseGDL* res=instance;
	
  ProgNode::interpreter->_retTree = rTree;
  return res;
}



BaseGDL* NSTRUCNode::Eval()
{
  ProgNodeP id = NULL;
  ProgNodeP i = NULL;
  ProgNodeP ii = NULL;
	
  DStructDesc*          nStructDesc;
  auto_ptr<DStructDesc> nStructDescGuard;
  BaseGDL* e;
  BaseGDL* ee;
	
	
  ProgNodeP n =  this;
  // 	match(antlr::RefAST(_t),NSTRUC);
  ProgNodeP _t = this->getFirstChild();
  id = _t;
  // 	match(antlr::RefAST(_t),IDENTIFIER);
  _t = _t->getNextSibling();
	
  // definedStruct: no tags present
  if( n->structDefined == 1) ProgNode::interpreter->GetStruct( id->getText(), _t);
	
  // find struct 'id' (for compatibility check)
  DStructDesc* oStructDesc=
    FindInStructList( structList, id->getText());
	
  if( oStructDesc == NULL || oStructDesc->NTags() > 0)
    {
      // not defined at all yet (-> define now)
      // or completely defined  (-> define now and check equality)
      nStructDesc= new DStructDesc( id->getText());
	
      // guard it
      nStructDescGuard.reset( nStructDesc); 
    } 
  else
    {   // NTags() == 0
	// not completely defined (only name in list)
      nStructDesc= oStructDesc;
    }
	
  // the instance variable
  //                 DStructGDL* instance= new DStructGDL( nStructDesc,
  //                                                       dimension(1)); 
  DStructGDL* instance= new DStructGDL( nStructDesc);
	
  auto_ptr<DStructGDL> instance_guard(instance);
	
  while( _t != NULL) 
    {
      switch ( _t->getType()) 
	{
	case DInterpreter::IDENTIFIER:
	  {
	    i = _t;
	    // 			match(antlr::RefAST(_t),IDENTIFIER);
	    _t = _t->getNextSibling();
	    e=_t->Eval();//expr(_t);
	    _t = _t->getNextSibling();
	    //WRONG  _t = ProgNode::interpreter->_retTree;
			
	    // also adds to descriptor, grabs
	    instance->NewTag( i->getText(), e); 
			
	    break;
	  }
	case DInterpreter::INHERITS:
	  {
	    //ProgNodeP tmp61_AST_in = _t;
	    // 			match(antlr::RefAST(_t),INHERITS);
	    _t = _t->getNextSibling();
	    ii = _t;
	    // 			match(antlr::RefAST(_t),IDENTIFIER);
	    _t = _t->getNextSibling();
			
	    DStructDesc* inherit=ProgNode::interpreter->GetStruct( ii->getText(), _t);
			
	    //   nStructDesc->AddParent( inherit);
	    instance->AddParent( inherit);
			
	    break;
	  }
	default:
	  /*		case ASSIGN:
			case ASSIGN_REPLACE:
			case ARRAYDEF:
			case ARRAYEXPR:
			case CONSTANT:
			case DEREF:
			case EXPR:
			case FCALL:
			case FCALL_LIB:
			case FCALL_LIB_RETNEW:
			case MFCALL:
			case MFCALL_PARENT:
			case NSTRUC:
			case NSTRUC_REF:
			case POSTDEC:
			case POSTINC:
			case STRUC:
			case SYSVAR:
			case VAR:
			case VARPTR:
			case DEC:
			case INC:
			case DOT:
			case QUESTION:*/
	  {
	    ee=_t->Eval();//expr(_t);
	    _t = _t->getNextSibling();
	    //WRONG _t = ProgNode::interpreter->_retTree;
			
	    // also adds to descriptor, grabs
	    instance->NewTag( 
			     oStructDesc->TagName( nStructDesc->NTags()),
			     ee);
			
	    break;
	  }
	} // switch
    } // while
	
  // inherit refers to nStructDesc, in case of error both have to
  // be freed here
  if( oStructDesc != NULL)
    {
      if( oStructDesc != nStructDesc)
	{
	  oStructDesc->AssureIdentical(nStructDesc);
	  instance->DStructGDL::SetDesc(oStructDesc);
	  //delete nStructDesc; // auto_ptr
	}
    }
  else
    {
      // release from guard (if not NULL)
      nStructDescGuard.release();
	
      // insert into struct list 
      structList.push_back(nStructDesc);
    }
	
  instance_guard.release();
  BaseGDL* res=instance;
	
  ProgNode::interpreter->_retTree = this->getNextSibling();
  return res;
}
BaseGDL* NSTRUC_REFNode::Eval()
{
   if( this->dStruct == NULL)
     {
      //   ProgNodeP rTree = this->getNextSibling();
      // 	match(antlr::RefAST(_t),NSTRUC_REF);
      ProgNodeP _t = this->getFirstChild();
      ProgNodeP idRef = _t;
      // 	match(antlr::RefAST(_t),IDENTIFIER);
      _t = _t->getNextSibling();
      
      // find struct 'id'
      // returns it or throws an exception
      /* DStructDesc* */ dStruct=ProgNode::interpreter->GetStruct( idRef->getText(), _t);
     }		

  BaseGDL* res = new DStructGDL( dStruct, dimension(1));
	
  ProgNode::interpreter->_retTree = this->getNextSibling();
  return res;
}



void KEYDEF_REFNode::Parameter( EnvBaseT* actEnv)
{
  ProgNodeP _t = this->getFirstChild();
//   ProgNodeP knameR = _t;
  // 			match(antlr::RefAST(_t),IDENTIFIER);
//   _t = _t->getNextSibling();
  BaseGDL** kvalRef=ProgNode::interpreter->ref_parameter(_t->getNextSibling());

  // pass reference
  actEnv->SetKeyword( _t->getText(), kvalRef); 
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}

void KEYDEF_REF_EXPRNode::Parameter( EnvBaseT* actEnv)
{
  ProgNodeP _t = this->getFirstChild();
//   ProgNodeP knameE = _t;
  // 			match(antlr::RefAST(_t),IDENTIFIER);
//   _t = _t->getNextSibling();
  BaseGDL* kval= _t->getNextSibling()->Eval();//expr(_t);
  delete kval;

//   _t = ProgNode::interpreter->_retTree;
  BaseGDL** kvalRef=ProgNode::interpreter->
    ref_parameter(ProgNode::interpreter->_retTree);

  // pass reference
  actEnv->SetKeyword( _t->getText(), kvalRef); 
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}
void KEYDEFNode::Parameter( EnvBaseT* actEnv)
{
   ProgNodeP _t = this->getFirstChild();
  // 			match(antlr::RefAST(_t),IDENTIFIER);
//   _t = _t->getNextSibling();
  BaseGDL* kval= _t->getNextSibling()->Eval();//expr(_t);
//   _t = ProgNode::interpreter->_retTree;
  // pass value
  actEnv->SetKeyword( _t->getText(), kval);
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}
void REFNode::Parameter( EnvBaseT* actEnv)
{
//   ProgNodeP _t = this->getFirstChild();
  BaseGDL** pvalRef=ProgNode::interpreter->ref_parameter(this->getFirstChild());
//   _t = ProgNode::interpreter->_retTree;
  // pass reference
  actEnv->SetNextPar(pvalRef); 
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}
void REF_EXPRNode::Parameter( EnvBaseT* actEnv)
{
  // 			match(antlr::RefAST(_t),REF_EXPR);
//   ProgNodeP _t = this->getFirstChild();
  BaseGDL* pval= this->getFirstChild()->Eval();//expr(_t);
  delete pval;
//   _t = ProgNode::interpreter->_retTree;
  BaseGDL** pvalRef=ProgNode::interpreter->
    ref_parameter( ProgNode::interpreter->_retTree);

  // pass reference
  actEnv->SetNextPar(pvalRef); 
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}
void KEYDEF_REF_CHECKNode::Parameter( EnvBaseT* actEnv)
{
//   ProgNodeP _t = this->getFirstChild();
//   ProgNodeP knameCk = _t;
  // 			match(antlr::RefAST(_t),IDENTIFIER);
//   _t = _t->getNextSibling();
  BaseGDL* kval=ProgNode::interpreter->
    lib_function_call(this->getFirstChild()->getNextSibling());
			
  BaseGDL** kvalRef = ProgNode::interpreter->callStack.back()->GetPtrTo( kval);
  if( kvalRef != NULL)
    {   // pass reference
      actEnv->SetKeyword(this->getFirstChild()->getText(), kvalRef); 
    }
  else 
    {   // pass value
      actEnv->SetKeyword(this->getFirstChild()->getText(), kval); 
    }
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}
void REF_CHECKNode::Parameter( EnvBaseT* actEnv)
{
  BaseGDL* pval=ProgNode::interpreter->lib_function_call(this->getFirstChild());
			
  BaseGDL** pvalRef = ProgNode::interpreter->callStack.back()->GetPtrTo( pval);
  if( pvalRef != NULL)
    {   // pass reference
      actEnv->SetNextPar( pvalRef); 
    }
  else 
    {   // pass value
      actEnv->SetNextPar( pval); 
    }
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}

void ParameterNode::Parameter( EnvBaseT* actEnv)
{
//   BaseGDL* pval=this->Eval();//expr(this);
			
  // pass value
  actEnv->SetNextPar(this->getFirstChild()->Eval()); 
			
  ProgNode::interpreter->_retTree = this->getNextSibling();
}



RetCode  ASSIGNNode::Run()
{
  BaseGDL*  r;
  BaseGDL** l;
  auto_ptr<BaseGDL> r_guard;
	
  //     match(antlr::RefAST(_t),ASSIGN);
  ProgNodeP _t = this->getFirstChild();
  {
    switch ( _t->getType()) {
    case GDLTokenTypes::CONSTANT:
    case GDLTokenTypes::DEREF:
    case GDLTokenTypes::SYSVAR:
    case GDLTokenTypes:: VAR:
    case GDLTokenTypes::VARPTR:
      {
	r= ProgNode::interpreter->indexable_expr(_t);
	_t = ProgNode::interpreter->_retTree;
	break;
      }
    case GDLTokenTypes::FCALL_LIB:
      {
	r=ProgNode::interpreter->lib_function_call(_t);
	_t = ProgNode::interpreter->_retTree;
			
	if( !ProgNode::interpreter->callStack.back()->Contains( r)) 
	  r_guard.reset( r); // guard if no global data
			
	break;
      }
    default:
      {
	r=ProgNode::interpreter->indexable_tmp_expr(_t);
	_t = ProgNode::interpreter->_retTree;
	r_guard.reset( r);
	break;
      }
    }//switch
  }
  l=ProgNode::interpreter->l_expr(_t, r);

  ProgNode::interpreter->_retTree = this->getNextSibling();

  return RC_OK;
}



RetCode  ASSIGN_ARRAYEXPR_MFCALLNode::Run()
{
  BaseGDL*  r;
  BaseGDL** l;
  auto_ptr<BaseGDL> r_guard;

  //match(antlr::RefAST(_t),ASSIGN_REPLACE);
  ProgNodeP _t = this->getFirstChild();
  {
    // BOTH
    if( _t->getType() ==  GDLTokenTypes::FCALL_LIB)
      {
		r=ProgNode::interpreter->lib_function_call(_t);

		if( r == NULL) // ROUTINE_NAMES
			ProgNode::interpreter->callStack.back()->Throw( "Undefined return value");
	
		_t = ProgNode::interpreter->_retTree;
		
		if( !ProgNode::interpreter->callStack.back()->Contains( r)) 
			r_guard.reset( r);
			
      }
    else
      {
			// ASSIGN
			switch ( _t->getType()) {
				case GDLTokenTypes::CONSTANT:
				case GDLTokenTypes::DEREF:
				case GDLTokenTypes::SYSVAR:
				case GDLTokenTypes:: VAR:
				case GDLTokenTypes::VARPTR:
				{
				r= ProgNode::interpreter->indexable_expr(_t);
				_t = ProgNode::interpreter->_retTree;
				break;
				}
				default:
				{
				r=ProgNode::interpreter->indexable_tmp_expr(_t);
				_t = ProgNode::interpreter->_retTree;
				r_guard.reset( r);
				break;
			}
			}//switch
		}
  }

	ProgNodeP lExpr = _t;
    
	// try MFCALL
	try
    {
    l=ProgNode::interpreter->l_arrayexpr_mfcall_as_mfcall(_t);
    
	if( r != (*l))
		{
		delete *l;

		if( r_guard.get() == r)
		*l = r_guard.release();
		else
		*l = r->Dup();
		}
    }
    catch( GDLException& e)
    {
		// try ARRAYEXPR
		try
		{
			l=ProgNode::interpreter->l_arrayexpr_mfcall_as_arrayexpr(lExpr, r);
		}
		catch( GDLException& e2)
		{
			throw GDLException(e.toString() + " or "+e2.toString());
		}
    }

  ProgNode::interpreter->_retTree = this->getNextSibling();
  return RC_OK;
}



RetCode  ASSIGN_REPLACENode::Run()
{
  BaseGDL*  r;
  BaseGDL** l;
  auto_ptr<BaseGDL> r_guard;

  //match(antlr::RefAST(_t),ASSIGN_REPLACE);
  ProgNodeP _t = this->getFirstChild();
  {
    if( _t->getType() ==  GDLTokenTypes::FCALL_LIB)
      {
		r=_t->Eval();//ProgNode::interpreter->lib_function_call(_t);
		r_guard.reset( r);

		if( r == NULL) // ROUTINE_NAMES
			throw GDLException( this, "Undefined return value", true, false);
		
		_t = ProgNode::interpreter->_retTree;
		
/*		if( !ProgNode::interpreter->callStack.back()->Contains( r))
			r_guard.reset( r);
		else
			r_guard.reset( r->Dup());*/
      }
    else
      {
			r=ProgNode::interpreter->tmp_expr(_t);
			_t = ProgNode::interpreter->_retTree;
					
			r_guard.reset( r);
      }
  }

  switch ( _t->getType()) {
  case GDLTokenTypes::VAR:
  case GDLTokenTypes::VARPTR:
    {
      l=ProgNode::interpreter->l_simple_var(_t);
//       _t = ProgNode::interpreter->_retTree;
      break;
    }
  case GDLTokenTypes::DEREF:
    {
      l=ProgNode::interpreter->l_deref(_t);
//       _t = ProgNode::interpreter->_retTree;
      break;
    }
  default:
//   case GDLTokenTypes::FCALL:
//   case GDLTokenTypes::FCALL_LIB:
//   case GDLTokenTypes::MFCALL:
//   case GDLTokenTypes::MFCALL_PARENT:
    {
      l=ProgNode::interpreter->l_function_call(_t);
//       _t = ProgNode::interpreter->_retTree;
      break;
    }
  }
//  if( r != (*l))
//     {
    delete *l;
//      if( r_guard.get() == r)
	*l = r_guard.release();
//       else
// 	*l = r->Dup();
//     }

  ProgNode::interpreter->SetRetTree( this->getNextSibling());
  return RC_OK;
}



RetCode  PCALL_LIBNode::Run()
{
  // better than auto_ptr: auto_ptr wouldn't remove newEnv from the stack
  StackGuard<EnvStackT> guard( ProgNode::interpreter->CallStack());
  BaseGDL *self;
	
  // 		match(antlr::RefAST(_t),PCALL_LIB);
  ProgNodeP _t = this->getFirstChild();
  ProgNodeP pl = _t;
  // 		match(antlr::RefAST(_t),IDENTIFIER);
  _t = _t->getNextSibling();
		
  EnvT* newEnv=new EnvT( pl, pl->libPro);//libProList[pl->proIx]);
		
  ProgNode::interpreter->parameter_def(_t, newEnv);
  //   _t = _retTree;
  //if( this->getLine() != 0) ProgNode::interpreter->callStack.back()->SetLineNumber( this->getLine());
		
  // push environment onto call stack
  ProgNode::interpreter->callStack.push_back(newEnv);
		
  // make the call
  static_cast<DLibPro*>(newEnv->GetPro())->Pro()(newEnv);

  ProgNode::interpreter->SetRetTree( this->getNextSibling());
  //  ProgNode::interpreter->_retTree = this->getNextSibling();
  return RC_OK;
}



RetCode  MPCALLNode::Run()
{
  // better than auto_ptr: auto_ptr wouldn't remove newEnv from the stack
  StackGuard<EnvStackT> guard(ProgNode::interpreter->CallStack());
  BaseGDL *self;
  EnvUDT*   newEnv;
	
  // 			match(antlr::RefAST(_t),MPCALL);
  ProgNodeP _t = this->getFirstChild();
  self=ProgNode::interpreter->expr(_t);
  _t = ProgNode::interpreter->_retTree;
  ProgNodeP mp = _t;
  // 			match(antlr::RefAST(_t),IDENTIFIER);
  _t = _t->getNextSibling();
			
  auto_ptr<BaseGDL> self_guard(self);
			
  newEnv=new EnvUDT( mp, self);
			
  self_guard.release();
			
  ProgNode::interpreter->parameter_def(_t, newEnv);

  //if( this->getLine() != 0) ProgNode::interpreter->callStack.back()->SetLineNumber( this->getLine());

  // push environment onto call stack
  ProgNode::interpreter->callStack.push_back(newEnv);
		
  // make the call
  ProgNode::interpreter->call_pro(static_cast<DSubUD*>(newEnv->GetPro())->GetTree());

  ProgNode::interpreter->SetRetTree( this->getNextSibling());
  return RC_OK;
}



RetCode  MPCALL_PARENTNode::Run()
{
  // better than auto_ptr: auto_ptr wouldn't remove newEnv from the stack
  StackGuard<EnvStackT> guard(ProgNode::interpreter->callStack);
  BaseGDL *self;
  EnvUDT*   newEnv;

  // 			match(antlr::RefAST(_t),MPCALL_PARENT);
  ProgNodeP _t = this->getFirstChild();
  self=ProgNode::interpreter->expr(_t);
  _t = ProgNode::interpreter->_retTree;
  ProgNodeP parent = _t;
  // 			match(antlr::RefAST(_t),IDENTIFIER);
  _t = _t->getNextSibling();
  ProgNodeP pp = _t;
  // 			match(antlr::RefAST(_t),IDENTIFIER);
  _t = _t->getNextSibling();
			
  auto_ptr<BaseGDL> self_guard(self);
			
  newEnv = new EnvUDT( pp, self, parent->getText());
			
  self_guard.release();
			
  ProgNode::interpreter->parameter_def(_t, newEnv);

  //if( this->getLine() != 0) ProgNode::interpreter->callStack.back()->SetLineNumber( this->getLine());

  // push environment onto call stack
  ProgNode::interpreter->callStack.push_back(newEnv);
		
  // make the call
  ProgNode::interpreter->call_pro(static_cast<DSubUD*>(newEnv->GetPro())->GetTree());

  ProgNode::interpreter->SetRetTree( this->getNextSibling());
  //  ProgNode::interpreter->_retTree = this->getNextSibling();
  return RC_OK;
}
RetCode  PCALLNode::Run()
{
  // better than auto_ptr: auto_ptr wouldn't remove newEnv from the stack
  StackGuard<EnvStackT> guard(ProgNode::interpreter->callStack);
  EnvUDT*   newEnv;
	
  // 			match(antlr::RefAST(_t),PCALL);
  ProgNodeP _t = this->getFirstChild();
  ProgNodeP p = _t;
  // 			match(antlr::RefAST(_t),IDENTIFIER);
  _t = _t->getNextSibling();
			
  ProgNode::interpreter->SetProIx( p);
			
  newEnv = new EnvUDT( p, proList[p->proIx]);
			
  ProgNode::interpreter->parameter_def(_t, newEnv);

//     if( _t->getLine() != 0) 
  //if( this->getLine() != 0) ProgNode::interpreter->callStack.back()->SetLineNumber( this->getLine());
// push environment onto call stack
  ProgNode::interpreter->callStack.push_back(newEnv);
		
  // make the call
  ProgNode::interpreter->call_pro(static_cast<DSubUD*>(newEnv->GetPro())->GetTree());

  ProgNode::interpreter->SetRetTree( this->getNextSibling());
  //  ProgNode::interpreter->_retTree = this->getNextSibling();
  return RC_OK;
}



RetCode  DECNode::Run()
{
  //		match(antlr::RefAST(_t),DEC);
  ProgNodeP _t = this->getFirstChild();
  ProgNode::interpreter->l_decinc_expr(_t, GDLTokenTypes::DECSTATEMENT);

  ProgNode::interpreter->SetRetTree( this->getNextSibling());
  return RC_OK;
}



RetCode  INCNode::Run()
{
  //		match(antlr::RefAST(_t),INC);
  ProgNodeP _t = this->getFirstChild();
  ProgNode::interpreter->l_decinc_expr(_t, GDLTokenTypes::INCSTATEMENT);

  ProgNode::interpreter->SetRetTree( this->getNextSibling());
  return RC_OK;
}

RetCode   FORNode::Run()//for_statement(ProgNodeP _t) {
{
		EnvUDT* callStack_back = 	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back());
		
		ForLoopInfoT& loopInfo = callStack_back->GetForLoopInfo( this->forLoopIx);
	
		ProgNodeP vP = this->GetNextSibling()->GetFirstChild();
		
		BaseGDL** v=ProgNode::interpreter->l_simple_var(vP);
		
		BaseGDL* s=ProgNode::interpreter->expr( this->GetFirstChild());
		auto_ptr<BaseGDL> s_guard(s);
		
		delete loopInfo.endLoopVar;
		loopInfo.endLoopVar=ProgNode::interpreter->expr(this->GetFirstChild()->GetNextSibling());
		
		s->ForCheck( &loopInfo.endLoopVar);
		
		// ASSIGNMENT used here also
		delete (*v);
		(*v)= s_guard.release(); // s held in *v after this
		
		if( (*v)->ForCondUp( loopInfo.endLoopVar))
		{
			ProgNode::interpreter->_retTree = vP->GetNextSibling();
			return RC_OK;
		}
		else
		{
			// skip if initial test fails
			ProgNode::interpreter->_retTree = this->GetNextSibling()->GetNextSibling();
			return RC_OK;
		}
}


	RetCode   FOR_LOOPNode::Run()
	{
		EnvUDT* callStack_back = 	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back());
 		ForLoopInfoT& loopInfo = 	callStack_back->GetForLoopInfo( this->forLoopIx);
		BaseGDL* endLoopVar = 	loopInfo.endLoopVar;
		if( endLoopVar == NULL)
		{
			// non-initialized loop (GOTO)
			ProgNode::interpreter->_retTree = this->GetNextSibling();
			return RC_OK;

		}

		// // problem:
		// // EXECUTE may call DataListT.loc.resize(), as v points to the
		// // old sequence v might be invalidated -> segfault
		// // note that the value (*v) is preserved by resize()
		
		BaseGDL** v=ProgNode::interpreter->l_simple_var(this->getFirstChild());

// shortCut:;
		
		//(*v)->ForAdd();
		if( (*v)->ForAddCondUp( endLoopVar))
		{
			ProgNode::interpreter->_retTree = this->statementList; //GetFirstChild()->GetNextSibling();
// 			if( ProgNode::interpreter->_retTree == this) goto shortCut;
		}
		else
		{
			delete loopInfo.endLoopVar;
			loopInfo.endLoopVar = NULL;
			ProgNode::interpreter->_retTree = this->GetNextSibling();
		}
		return RC_OK;
	}

	
 RetCode   FOR_STEPNode::Run()//for_statement(ProgNodeP _t) {
{
	EnvUDT* callStack_back = 	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back());

	ForLoopInfoT& loopInfo = callStack_back->GetForLoopInfo( this->forLoopIx);

	ProgNodeP vP = this->GetNextSibling()->GetFirstChild();

	BaseGDL** v=ProgNode::interpreter->l_simple_var(vP);

	BaseGDL* s=ProgNode::interpreter->expr( this->GetFirstChild());
	auto_ptr<BaseGDL> s_guard(s);

	delete loopInfo.endLoopVar;
	loopInfo.endLoopVar=ProgNode::interpreter->expr(this->GetFirstChild()->GetNextSibling());

	delete loopInfo.loopStepVar;
	loopInfo.loopStepVar=ProgNode::interpreter->expr(this->GetFirstChild()->GetNextSibling()->GetNextSibling());

	s->ForCheck( &loopInfo.endLoopVar, &loopInfo.loopStepVar);

	// ASSIGNMENT used here also
	delete (*v);
	(*v)= s_guard.release(); // s held in *v after this

	if( loopInfo.loopStepVar->Sgn() == -1)
	{
		if( (*v)->ForCondDown( loopInfo.endLoopVar))
		{
			ProgNode::interpreter->_retTree = vP->GetNextSibling();
  return RC_OK;

		}
	}
	else
	{
		if( (*v)->ForCondUp( loopInfo.endLoopVar))
		{
			ProgNode::interpreter->_retTree = vP->GetNextSibling();
  return RC_OK;

		}
	}
	// skip if initial test fails
	ProgNode::interpreter->_retTree = this->GetNextSibling()->GetNextSibling();
  return RC_OK;
}
	
RetCode   FOR_STEP_LOOPNode::Run()
{
	EnvUDT* callStack_back = 	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back());
	
	ForLoopInfoT& loopInfo = 	callStack_back->GetForLoopInfo( this->forLoopIx);
	if( loopInfo.endLoopVar == NULL)
	{
		// non-initialized loop (GOTO)
		ProgNode::interpreter->_retTree = this->GetNextSibling();
        return RC_OK;
	}

	// // problem:
	// // EXECUTE may call DataListT.loc.resize(), as v points to the
	// // old sequence v might be invalidated -> segfault
	// // note that the value (*v) is preserved by resize()

	BaseGDL** v=ProgNode::interpreter->l_simple_var(this->GetFirstChild());

	(*v)->ForAdd(loopInfo.loopStepVar);
	if( loopInfo.loopStepVar->Sgn() == -1)
	{
		if( (*v)->ForCondDown( loopInfo.endLoopVar))
		{
			ProgNode::interpreter->_retTree = this->GetFirstChild()->GetNextSibling();
  return RC_OK;
		}
	}
	else
	{
		if( (*v)->ForCondUp( loopInfo.endLoopVar))
		{
			ProgNode::interpreter->_retTree = this->GetFirstChild()->GetNextSibling();
  return RC_OK;
		}
	}
	
	delete loopInfo.endLoopVar;
	loopInfo.endLoopVar = NULL;
	delete loopInfo.loopStepVar;
	loopInfo.loopStepVar = NULL;
	ProgNode::interpreter->_retTree = this->GetNextSibling();
	return RC_OK;
}

RetCode   FOREACHNode::Run()
{
	EnvUDT* callStack_back = 	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back());
	ForLoopInfoT& loopInfo = callStack_back->GetForLoopInfo( this->forLoopIx);

	ProgNodeP vP = this->GetNextSibling()->GetFirstChild();

	BaseGDL** v=ProgNode::interpreter->l_simple_var(vP);

	delete loopInfo.endLoopVar;
	loopInfo.endLoopVar=ProgNode::interpreter->expr(this->GetFirstChild());

	loopInfo.foreachIx = 0;

	// currently there are no empty arrays
	//SizeT nEl = loopInfo.endLoopVar->N_Elements();

	// ASSIGNMENT used here also
	delete (*v);
	(*v) = loopInfo.endLoopVar->NewIx( 0);

	ProgNode::interpreter->_retTree = vP->GetNextSibling();
	return RC_OK;
}
	
RetCode   FOREACH_LOOPNode::Run()
{
	EnvUDT* callStack_back = 	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back());
	ForLoopInfoT& loopInfo = callStack_back->GetForLoopInfo( this->forLoopIx);

	if( loopInfo.endLoopVar == NULL)
	{
	// non-initialized loop (GOTO)
	ProgNode::interpreter->_retTree = this->GetNextSibling();
  return RC_OK;
	}

	BaseGDL** v=ProgNode::interpreter->l_simple_var(this->GetFirstChild());
	
	++loopInfo.foreachIx;

	SizeT nEl = loopInfo.endLoopVar->N_Elements();

	if( loopInfo.foreachIx < nEl)
	{
		// ASSIGNMENT used here also
		delete (*v);
		(*v) = loopInfo.endLoopVar->NewIx( loopInfo.foreachIx);

		ProgNode::interpreter->_retTree = this->GetFirstChild()->GetNextSibling();
  return RC_OK;
	}

	delete loopInfo.endLoopVar;
	loopInfo.endLoopVar = NULL;
	// 	loopInfo.foreachIx = -1;
	ProgNode::interpreter->SetRetTree( this->GetNextSibling());
  return RC_OK;
}



RetCode   REPEATNode::Run()
{
	// _t is REPEAT_LOOP, GetFirstChild() is expr, GetNextSibling is first loop statement
	if( this->GetFirstChild()->GetFirstChild()->GetNextSibling() == NULL)
		ProgNode::interpreter->SetRetTree( this->GetFirstChild());
	else	
		ProgNode::interpreter->SetRetTree( this->GetFirstChild()->GetFirstChild()->GetNextSibling());     // statement
  return RC_OK;
}



RetCode   REPEAT_LOOPNode::Run()
{
	auto_ptr<BaseGDL> eVal( ProgNode::interpreter->expr(this->GetFirstChild()));
	if( eVal.get()->False())
	{
	ProgNode::interpreter->SetRetTree( this->GetFirstChild()->GetNextSibling());     // 1st loop statement
	if(  this->GetFirstChild()->GetNextSibling() == NULL)
		throw GDLException(this,	"Empty REPEAT loop entered (infinite loop).",true,false);
  return RC_OK;
	}
	
	ProgNode::interpreter->SetRetTree( this->GetNextSibling());     // statement
  return RC_OK;
}



RetCode   WHILENode::Run()
{
  auto_ptr<BaseGDL> e1_guard;
  BaseGDL* e1;
  ProgNodeP evalExpr = this->getFirstChild();
  if( NonCopyNode( evalExpr->getType()))
  {
	e1 = evalExpr->EvalNC();
  }
  else
  {
	e1 = evalExpr->Eval();
    e1_guard.reset(e1);
  }
// 	auto_ptr<BaseGDL> eVal( ProgNode::interpreter->expr( this->GetFirstChild()));
	if( e1->True()) 
	{
		ProgNode::interpreter->SetRetTree( this->GetFirstChild()->GetNextSibling());
		if( this->GetFirstChild()->GetNextSibling() == NULL)
			throw GDLException(this,"Empty WHILE loop entered (infinite loop).",true,false);
	}
	else
	{
		ProgNode::interpreter->SetRetTree( this->GetNextSibling());
	}
  return RC_OK;
}



RetCode   IFNode::Run()
 {
  auto_ptr<BaseGDL> e1_guard;
  BaseGDL* e1;
  ProgNodeP evalExpr = this->getFirstChild();
  if( NonCopyNode( evalExpr->getType()))
  {
	e1 = evalExpr->EvalNC();
  }
  else
  {
	e1 = evalExpr->Eval();
    e1_guard.reset(e1);
  }
//	auto_ptr<BaseGDL> eVal( ProgNode::interpreter->expr( this->GetFirstChild()));
	if( e1->True()) 
	{
		ProgNode::interpreter->SetRetTree( this->GetFirstChild()->GetNextSibling());
	}
	else
	{
		ProgNode::interpreter->SetRetTree( this->GetNextSibling());
	}
  return RC_OK;
}

RetCode   IF_ELSENode::Run()
{	
  auto_ptr<BaseGDL> e1_guard;
  BaseGDL* e1;
  ProgNodeP evalExpr = this->getFirstChild();
  if( NonCopyNode( evalExpr->getType()))
  {
	e1 = evalExpr->EvalNC();
  }
  else
  {
	e1 = evalExpr->Eval();
    e1_guard.reset(e1);
  }
// 	auto_ptr<BaseGDL> eVal( ProgNode::interpreter->expr( this->GetFirstChild()));
	if( e1->True()) 
	{
		ProgNode::interpreter->SetRetTree( this->GetFirstChild()->GetNextSibling()->GetFirstChild());
	}
	else
	{
		ProgNode::interpreter->SetRetTree( this->GetFirstChild()->GetNextSibling()->GetNextSibling());
	}
  return RC_OK;
}



RetCode   CASENode::Run()
{
  auto_ptr<BaseGDL> e1_guard;
  BaseGDL* e1;
  ProgNodeP evalExpr = this->getFirstChild();
  if( NonCopyNode( evalExpr->getType()))
  {
	e1 = evalExpr->EvalNC();
  }
  else
  {
	e1 = evalExpr->Eval();
    e1_guard.reset(e1);
  }

// 	auto_ptr<BaseGDL> eVal( ProgNode::interpreter->expr( this->GetFirstChild()));
  	if( !e1->Scalar())
		throw GDLException( this->GetFirstChild(), "Expression must be a"
			" scalar in this context: "+ProgNode::interpreter->Name(e1),true,false);

	ProgNodeP b=this->GetFirstChild()->GetNextSibling(); // remeber block begin
 
	for( int i=0; i<this->numBranch; ++i)
	{
		if( b->getType() == GDLTokenTypes::ELSEBLK)
		{
			ProgNodeP sL = b->GetFirstChild(); // statement_list
		
			if(sL != NULL )
			{
				ProgNode::interpreter->SetRetTree( sL);
			}
			else
			{
				ProgNode::interpreter->SetRetTree( this->GetNextSibling());
			}
			return RC_OK;
		}
		else
		{
			ProgNodeP ex = b->GetFirstChild();  // EXPR
							
			auto_ptr<BaseGDL> ee_guard;
			BaseGDL* ee;
			if( NonCopyNode( ex->getType()))
				{
					ee = ex->EvalNC();
				}
			else
				{
					ee = ex->Eval();
					ee_guard.reset(ee);
				}
// 			BaseGDL* ee=ProgNode::interpreter->expr(ex);
			// auto_ptr<BaseGDL> ee_guard(ee);
			bool equalexpr=e1->EqualNoDelete(ee); // Equal deletes ee
		
			if( equalexpr)
			{
				ProgNodeP bb = ex->GetNextSibling(); // statement_list
				if(bb != NULL )
				{
					ProgNode::interpreter->SetRetTree( bb);
					return RC_OK;
				}
				else
				{
					ProgNode::interpreter->SetRetTree( this->GetNextSibling());
					return RC_OK;
				}
			}
		}
		b=b->GetNextSibling(); // next block
	} // for
	
	throw GDLException( this, "CASE statement found no match.",true,false);
	return RC_OK;
}



RetCode   SWITCHNode::Run()
{
  auto_ptr<BaseGDL> e1_guard;
  BaseGDL* e1;
  ProgNodeP evalExpr = this->getFirstChild();
  if( NonCopyNode( evalExpr->getType()))
  {
	e1 = evalExpr->EvalNC();
  }
  else
  {
	e1 = evalExpr->Eval();
    e1_guard.reset(e1);
  }

// 	auto_ptr<BaseGDL> eVal( ProgNode::interpreter->expr( this->GetFirstChild()));
 	if( !e1->Scalar())
	throw GDLException( this->GetFirstChild(), "Expression must be a"
	" scalar in this context: "+ProgNode::interpreter->Name(e1),true,false);

	ProgNodeP b=this->GetFirstChild()->GetNextSibling(); // remeber block begin
	
	bool hook=false; // switch executes everything after 1st match
	for( int i=0; i<this->numBranch; i++)
	{
		if( b->getType() == GDLTokenTypes::ELSEBLK)
			{
				hook=true;
				
				ProgNodeP sL = b->GetFirstChild(); // statement_list
				
				if(sL != NULL )
				{
					ProgNode::interpreter->SetRetTree( sL);
					return RC_OK;
				}
			}
		else
			{
				ProgNodeP ex = b->GetFirstChild();  // EXPR
				
				if( !hook)
				{
					auto_ptr<BaseGDL> ee_guard;
					BaseGDL* ee;
					if( NonCopyNode( ex->getType()))
					{
						ee = ex->EvalNC();
					}
					else
					{
						ee = ex->Eval();
						ee_guard.reset(ee);
					}
// 					BaseGDL* ee=ProgNode::interpreter->expr(ex);
					// auto_ptr<BaseGDL> ee_guard(ee);
					hook=e1->EqualNoDelete(ee); // Equal deletes ee
				}
				
				if( hook)
				{
					ProgNodeP bb = ex->GetNextSibling(); // statement_list
					// statement there
					if(bb != NULL )
					{
						ProgNode::interpreter->SetRetTree( bb);
						return RC_OK;
					}
				}
			}
		b=b->GetNextSibling(); // next block
	} // for
	ProgNode::interpreter->SetRetTree( this->GetNextSibling());
	return RC_OK;
}

RetCode   BLOCKNode::Run()
{
	ProgNode::interpreter->SetRetTree( this->getFirstChild());
  return RC_OK;
}

RetCode      GOTONode::Run()
	{
		ProgNode::interpreter->SetRetTree( static_cast<EnvUDT*>(GDLInterpreter::CallStack().back())->
			GotoTarget( targetIx)->GetNextSibling());
  return RC_OK;
	}
RetCode      CONTINUENode::Run()
{
assert( this->breakTarget != NULL); ProgNode::interpreter->SetRetTree( this->breakTarget);
  return RC_OK;
}
RetCode      BREAKNode::Run()
{
ProgNode::interpreter->SetRetTree( this->breakTarget);
  return RC_OK;
}
RetCode      LABELNode::Run()
{
 ProgNode::interpreter->SetRetTree( this->GetNextSibling());
   return RC_OK;
}
RetCode      ON_IOERROR_NULLNode::Run()
{
	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back())->SetIOError( -1);
	ProgNode::interpreter->SetRetTree( this->GetNextSibling());
  return RC_OK;
}
RetCode      ON_IOERRORNode::Run()
{
	static_cast<EnvUDT*>(GDLInterpreter::CallStack().back())->SetIOError( this->targetIx);
	ProgNode::interpreter->SetRetTree( this->GetNextSibling());
  return RC_OK;
}


RetCode   RETFNode::Run()
{
	ProgNodeP _t = this->getFirstChild();
	assert( _t != NULL);
	if ( !static_cast<EnvUDT*>(GDLInterpreter::CallStack().back())->LFun())
		{
			BaseGDL* e=ProgNode::interpreter->expr(_t);

			delete ProgNode::interpreter->returnValue;
			ProgNode::interpreter->returnValue=e;

			GDLInterpreter::CallStack().back()->RemoveLoc( e); // steal e from local list
		}
	else
		{
			BaseGDL** eL=ProgNode::interpreter->l_ret_expr(_t);

			// returnValueL is otherwise owned
			ProgNode::interpreter->returnValueL=eL;
		}
	//if( !(interruptEnable && sigControlC) && ( debugMode == DEBUG_CLEAR))
	//return RC_RETURN;
	return RC_RETURN;
}

RetCode   RETPNode::Run()
{
	return RC_RETURN;
}







