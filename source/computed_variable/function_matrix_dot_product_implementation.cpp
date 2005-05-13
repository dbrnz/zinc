//******************************************************************************
// FILE : function_matrix_dot_product_implementation.cpp
//
// LAST MODIFIED : 12 April 2005
//
// DESCRIPTION :
//==============================================================================

#include <sstream>

#include "computed_variable/function_matrix_dot_product.hpp"
#include "computed_variable/function_variable.hpp"
#include "computed_variable/function_variable_matrix.hpp"
#include "computed_variable/function_variable_union.hpp"
#include "computed_variable/function_variable_value_specific.hpp"
#if defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
#else // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
#include "computed_variable/function_variable_composite.hpp"
#include "computed_variable/function_derivative.hpp"
#endif // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)


// module classes
// ==============

// class Function_variable_matrix_dot_product
// ------------------------------------------

#if defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
#else // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
// class Function_derivatnew_matrix_dot_product
// --------------------------------------------

EXPORT template<typename Value_type>
class Function_derivatnew_matrix_dot_product : public Function_derivatnew
//******************************************************************************
// LAST MODIFIED : 12 April 2005
//
// DESCRIPTION :
//==============================================================================
{
	public:
		// for construction exception
		class Construction_exception {};
		// constructor
		Function_derivatnew_matrix_dot_product(
			const Function_variable_handle& dependent_variable,
			const std::list<Function_variable_handle>& independent_variables);
		// destructor
		~Function_derivatnew_matrix_dot_product();
	// inherited
	private:
#if defined (EVALUATE_RETURNS_VALUE)
		virtual Function_handle evaluate(Function_variable_handle atomic_variable);
#else // defined (EVALUATE_RETURNS_VALUE)
		virtual bool evaluate(Function_variable_handle atomic_variable);
#endif // defined (EVALUATE_RETURNS_VALUE)
	private:
		Function_variable_handle intermediate_variable;
		Function_derivatnew_handle derivative_g;
};
#endif // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)

EXPORT template<typename Value_type>
class Function_variable_matrix_dot_product :
	public Function_variable_matrix<Value_type>
//******************************************************************************
// LAST MODIFIED : 12 April 2005
//
// DESCRIPTION :
//==============================================================================
{
	friend class Function_matrix<Value_type>;
	public:
		// constructor
		Function_variable_matrix_dot_product(
			const boost::intrusive_ptr< Function_matrix_dot_product<Value_type> >
			function_matrix_dot_product):
			Function_variable_matrix<Value_type>(function_matrix_dot_product,
#if defined (Function_variable_matrix_HAS_INPUT_ATTRIBUTE)
			false,
#endif // defined (Function_variable_matrix_HAS_INPUT_ATTRIBUTE)
			1,1){};
		~Function_variable_matrix_dot_product(){}
	public:
		Function_variable_handle clone() const
		{
			return (Function_variable_handle(
				new Function_variable_matrix_dot_product<Value_type>(*this)));
		};
#if defined (EVALUATE_RETURNS_VALUE)
		Function_handle evaluate()
		{
			Function_handle result(0);
			boost::intrusive_ptr< Function_matrix_dot_product<Value_type> >
				function_matrix_dot_product;

			if (function_matrix_dot_product=boost::dynamic_pointer_cast<
				Function_matrix_dot_product<Value_type>,Function>(this->function()))
			{
#if defined (BEFORE_CACHING)
				Function_size_type number_of_columns,number_of_rows;
				boost::intrusive_ptr< Function_matrix<Value_type> > variable_1,
					variable_2;

				if ((variable_1=boost::dynamic_pointer_cast<Function_matrix<Value_type>,
					Function>(function_matrix_dot_product->variable_1_private->
					evaluate()))&&
					(variable_2=boost::dynamic_pointer_cast<Function_matrix<Value_type>,
					Function>(function_matrix_dot_product->variable_2_private->
					evaluate()))&&
					((number_of_rows=variable_1->number_of_rows())==
					variable_2->number_of_rows())&&
					((number_of_columns=variable_1->number_of_columns())==
					variable_2->number_of_columns()))
				{
					Function_size_type i,j;
					Value_type sum;

					sum=0;
					for (i=1;i<=number_of_rows;++i)
					{
						for (j=1;j<=number_of_columns;++j)
						{
							sum += (*variable_1)(i,j)*(*variable_2)(i,j);
						}
					}
					function_matrix_dot_product->values(0,0)=sum;
					result=Function_handle(new Function_matrix<Value_type>(
						function_matrix_dot_product->values));
				}
#else // defined (BEFORE_CACHING)
				if (!(function_matrix_dot_product->evaluated()))
				{
					Function_size_type number_of_columns,number_of_rows;
					boost::intrusive_ptr< Function_matrix<Value_type> > variable_1,
						variable_2;

					if ((variable_1=boost::dynamic_pointer_cast<Function_matrix<
						Value_type>,Function>(function_matrix_dot_product->
						variable_1_private->evaluate()))&&
						(variable_2=boost::dynamic_pointer_cast<Function_matrix<Value_type>,
						Function>(function_matrix_dot_product->variable_2_private->
						evaluate()))&&
						((number_of_rows=variable_1->number_of_rows())==
						variable_2->number_of_rows())&&
						((number_of_columns=variable_1->number_of_columns())==
						variable_2->number_of_columns()))
					{
						Function_size_type i,j;
						Value_type sum;

						sum=0;
						for (i=1;i<=number_of_rows;++i)
						{
							for (j=1;j<=number_of_columns;++j)
							{
								sum += (*variable_1)(i,j)*(*variable_2)(i,j);
							}
						}
						function_matrix_dot_product->values(0,0)=sum;
						function_matrix_dot_product->set_evaluated();
					}
				}
				if (function_matrix_dot_product->evaluated())
				{
					result=Function_handle(new Function_matrix<Value_type>(
						function_matrix_dot_product->values));
				}
#endif // defined (BEFORE_CACHING)
			}

			return (result);
		};
#else // defined (EVALUATE_RETURNS_VALUE)
		bool evaluate()
		{
			bool result(true);
			boost::intrusive_ptr< Function_matrix_dot_product<Value_type> >
				function_matrix_dot_product;

			if (function_matrix_dot_product=boost::dynamic_pointer_cast<
				Function_matrix_dot_product<Value_type>,Function>(function()))
			{
#if defined (BEFORE_CACHING)
				Function_size_type number_of_columns,number_of_rows;
				boost::intrusive_ptr< Function_matrix<Value_type> > variable_1,
					variable_2;

				result=false;
				if ((function_matrix_dot_product->variable_1_private->evaluate)()&&
					(variable_1=boost::dynamic_pointer_cast<Function_matrix<Value_type>,
					Function>(function_matrix_dot_product->variable_1_private->
					get_value()))&&
					(function_matrix_dot_product->variable_2_private->evaluate)()&&
					(variable_2=boost::dynamic_pointer_cast<Function_matrix<Value_type>,
					Function>(function_matrix_dot_product->variable_2_private->
					get_value()))&&
					((number_of_rows=variable_1->number_of_rows())==
					variable_2->number_of_rows())&&
					((number_of_columns=variable_1->number_of_columns())==
					variable_2->number_of_columns()))
				{
					Function_size_type i,j;
					Value_type sum;

					sum=0;
					for (i=1;i<=number_of_rows;++i)
					{
						for (j=1;j<=number_of_columns;++j)
						{
							sum += (*variable_1)(i,j)*(*variable_2)(i,j);
						}
					}
					function_matrix_dot_product->values(0,0)=sum;
					result=true;
				}
#else // defined (BEFORE_CACHING)
				if (!(function_matrix_dot_product->evaluated()))
				{
					Function_size_type number_of_columns,number_of_rows;
					boost::intrusive_ptr< Function_matrix<Value_type> > variable_1,
						variable_2;

					result=false;
					if ((function_matrix_dot_product->variable_1_private->evaluate)()&&
						(variable_1=boost::dynamic_pointer_cast<Function_matrix<Value_type>,
						Function>(function_matrix_dot_product->variable_1_private->
						get_value()))&&
						(function_matrix_dot_product->variable_2_private->evaluate)()&&
						(variable_2=boost::dynamic_pointer_cast<Function_matrix<Value_type>,
						Function>(function_matrix_dot_product->variable_2_private->
						get_value()))&&
						((number_of_rows=variable_1->number_of_rows())==
						variable_2->number_of_rows())&&
						((number_of_columns=variable_1->number_of_columns())==
						variable_2->number_of_columns()))
					{
						Function_size_type i,j;
						Value_type sum;

						sum=0;
						for (i=1;i<=number_of_rows;++i)
						{
							for (j=1;j<=number_of_columns;++j)
							{
								sum += (*variable_1)(i,j)*(*variable_2)(i,j);
							}
						}
						function_matrix_dot_product->values(0,0)=sum;
						function_matrix_dot_product->set_evaluated();
						result=true;
					}
				}
#endif // defined (BEFORE_CACHING)
			}

			return (result);
		};
#endif // defined (EVALUATE_RETURNS_VALUE)
#if defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
		Function_handle evaluate_derivative(std::list<Function_variable_handle>&)
		{
			return (0);
		};
#else // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
		Function_handle derivative(
			const std::list<Function_variable_handle>& independent_variables)
		{
			return (Function_handle(
				new Function_derivatnew_matrix_dot_product<Value_type>(
				Function_variable_handle(this),independent_variables)));
		}
#endif // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
		//???DB.  Should operator() and get_entry do an evaluate?
		boost::intrusive_ptr< Function_variable_matrix<Value_type> > operator()(
			Function_size_type row=1,Function_size_type column=1) const
		{
			boost::intrusive_ptr< Function_matrix_dot_product<Value_type> >
				function_matrix_dot_product;
			boost::intrusive_ptr< Function_variable_matrix<Value_type> > result(0);

			if ((function_matrix_dot_product=boost::dynamic_pointer_cast<
				Function_matrix_dot_product<Value_type>,Function>(this->function_private))&&
				(row<=this->number_of_rows())&&(column<=this->number_of_columns()))
			{
				result=boost::intrusive_ptr< Function_variable_matrix<Value_type> >(
					new Function_variable_matrix_dot_product<Value_type>(
					function_matrix_dot_product));
			}

			return (result);
		}
	private:
		// copy constructor
		Function_variable_matrix_dot_product(
			const Function_variable_matrix_dot_product<Value_type>& variable):
			Function_variable_matrix<Value_type>(variable){};
};


#if defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
#else // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)
// class Function_derivatnew_matrix_dot_product
// --------------------------------------------

EXPORT template<typename Value_type>
Function_derivatnew_matrix_dot_product<Value_type>::
	Function_derivatnew_matrix_dot_product(
	const Function_variable_handle& dependent_variable,
	const std::list<Function_variable_handle>& independent_variables):
	Function_derivatnew(dependent_variable,independent_variables)
//******************************************************************************
// LAST MODIFIED : 12 April 2005
//
// DESCRIPTION :
// Constructor.
//==============================================================================
{
	boost::intrusive_ptr< Function_matrix_dot_product<Value_type> >
		function_matrix_dot_product;
	boost::intrusive_ptr< Function_variable_matrix_dot_product<Value_type> >
		variable_matrix_dot_product;

	if ((variable_matrix_dot_product=boost::dynamic_pointer_cast<
		Function_variable_matrix_dot_product<Value_type>,Function_variable>(
		dependent_variable))&&
		(function_matrix_dot_product=boost::dynamic_pointer_cast<
		Function_matrix_dot_product<Value_type>,Function>(
		dependent_variable->function())))
	{
		if (!((intermediate_variable=Function_variable_handle(
			new Function_variable_composite(
			function_matrix_dot_product->variable_1_private,
			function_matrix_dot_product->variable_2_private)))&&
			(derivative_g=boost::dynamic_pointer_cast<Function_derivatnew,
			Function>(intermediate_variable->derivative(independent_variables)))))
		{
			throw Function_derivatnew_matrix_dot_product::Construction_exception();
		}
	}
	else
	{
		throw Function_derivatnew_matrix_dot_product::Construction_exception();
	}
}

EXPORT template<typename Value_type>
Function_derivatnew_matrix_dot_product<Value_type>::
	~Function_derivatnew_matrix_dot_product(){}
//******************************************************************************
// LAST MODIFIED : 12 April 2005
//
// DESCRIPTION :
// Destructor.
//==============================================================================

EXPORT template<typename Value_type>
#if defined (EVALUATE_RETURNS_VALUE)
Function_handle
#else // defined (EVALUATE_RETURNS_VALUE)
bool
#endif // defined (EVALUATE_RETURNS_VALUE)
	Function_derivatnew_matrix_dot_product<Value_type>::evaluate(
	Function_variable_handle)
//******************************************************************************
// LAST MODIFIED : 12 April 2005
//
// DESCRIPTION :
//==============================================================================
{
	return (
#if defined (EVALUATE_RETURNS_VALUE)
		Function_handle(0);
#else // defined (EVALUATE_RETURNS_VALUE)
		false
#endif // defined (EVALUATE_RETURNS_VALUE)
		);
}
#endif // defined (USE_FUNCTION_VARIABLE__EVALUATE_DERIVATIVE)


// global classes
// ==============

// class Function_matrix_dot_product
// ---------------------------------

EXPORT template<typename Value_type>
ublas::matrix<Value_type,ublas::column_major> 
	Function_matrix_dot_product<Value_type>::constructor_values(1,1);

EXPORT template<typename Value_type>
Function_matrix_dot_product<Value_type>::Function_matrix_dot_product(
	const Function_variable_handle& variable_1,
	const Function_variable_handle& variable_2):Function_matrix<Value_type>(
	Function_matrix_dot_product<Value_type>::constructor_values),
	variable_1_private(variable_1),variable_2_private(variable_2)
//******************************************************************************
// LAST MODIFIED : 7 December 2004
//
// DESCRIPTION :
// Constructor.
//==============================================================================
{
	if (variable_1_private)
	{
		variable_1_private->add_dependent_function(this);
	}
	if (variable_2_private)
	{
		variable_2_private->add_dependent_function(this);
	}
}

EXPORT template<typename Value_type>
Function_matrix_dot_product<Value_type>::~Function_matrix_dot_product()
//******************************************************************************
// LAST MODIFIED : 7 December 2004
//
// DESCRIPTION :
// Destructor.
//==============================================================================
{
#if defined (CIRCULAR_SMART_POINTERS)
	// do nothing
#else // defined (CIRCULAR_SMART_POINTERS)
	if (variable_1_private)
	{
		variable_1_private->remove_dependent_function(this);
	}
	if (variable_2_private)
	{
		variable_2_private->remove_dependent_function(this);
	}
#endif // defined (CIRCULAR_SMART_POINTERS)
}

EXPORT template<typename Value_type>
	string_handle Function_matrix_dot_product<Value_type>::
	get_string_representation()
//******************************************************************************
// LAST MODIFIED : 19 October 2004
//
// DESCRIPTION :
//==============================================================================
{
	string_handle return_string;
	std::ostringstream out;

	if (return_string=new std::string)
	{
		if (variable_1_private&&variable_2_private)
		{
			out << "dot_product(";
			out << *(variable_1_private->get_string_representation());
			out << ",";
			out << *(variable_2_private->get_string_representation());
			out << ")";
		}
		else
		{
			out << "Invalid Function_matrix_dot_product";
		}
		*return_string=out.str();
	}

	return (return_string);
}

EXPORT template<typename Value_type>
Function_variable_handle Function_matrix_dot_product<Value_type>::input()
//******************************************************************************
// LAST MODIFIED : 19 October 2004
//
// DESCRIPTION :
//==============================================================================
{
	Function_handle function;
	Function_variable_handle input_1(0),input_2(0),result(0);

	if (variable_1_private&&(function=variable_1_private->function()))
	{
		input_1=function->input();
	}
	if (variable_2_private&&(function=variable_2_private->function()))
	{
		input_2=function->input();
	}
	if (input_1)
	{
		if (input_2)
		{
			result=Function_variable_handle(new Function_variable_union(
				Function_handle(this),input_1,input_2));
		}
		else
		{
			result=input_1;
		}
	}
	else
	{
		result=input_2;
	}

	return (result);
}

EXPORT template<typename Value_type>
Function_variable_handle Function_matrix_dot_product<Value_type>::output()
//******************************************************************************
// LAST MODIFIED : 19 October 2004
//
// DESCRIPTION :
//==============================================================================
{
	return (Function_variable_handle(
		new Function_variable_matrix_dot_product<Value_type>(
		boost::intrusive_ptr< Function_matrix_dot_product<Value_type> >(this))));
}

EXPORT template<typename Value_type>
bool Function_matrix_dot_product<Value_type>::operator==(
	const Function& function) const
//******************************************************************************
// LAST MODIFIED : 19 October 2004
//
// DESCRIPTION :
// Equality operator.
//==============================================================================
{
	bool result;

	result=false;
	if (this)
	{
		try
		{
			const Function_matrix_dot_product<Value_type>&
				function_matrix_dot_product=
				dynamic_cast<const Function_matrix_dot_product<Value_type>&>(function);

			result=equivalent(variable_1_private,
				function_matrix_dot_product.variable_1_private)&&
				equivalent(variable_2_private,
				function_matrix_dot_product.variable_2_private);
		}
		catch (std::bad_cast)
		{
			// do nothing
		}
	}

	return (result);
}

EXPORT template<typename Value_type>
#if defined (EVALUATE_RETURNS_VALUE)
Function_handle
#else // defined (EVALUATE_RETURNS_VALUE)
bool
#endif // defined (EVALUATE_RETURNS_VALUE)
	Function_matrix_dot_product<Value_type>::evaluate(
	Function_variable_handle atomic_variable)
//******************************************************************************
// LAST MODIFIED : 13 January 2005
//
// DESCRIPTION :
//==============================================================================
{
	boost::intrusive_ptr< Function_variable_matrix_dot_product<Value_type> >
		atomic_variable_matrix_dot_product;
#if defined (EVALUATE_RETURNS_VALUE)
	Function_handle result(0);
#else // defined (EVALUATE_RETURNS_VALUE)
	bool result(true);
#endif // defined (EVALUATE_RETURNS_VALUE)

	if ((atomic_variable_matrix_dot_product=boost::dynamic_pointer_cast<
		Function_variable_matrix_dot_product<Value_type>,Function_variable>(
		atomic_variable))&&equivalent(Function_handle(this),
		atomic_variable_matrix_dot_product->function())&&
		(1==atomic_variable_matrix_dot_product->row())&&
		(1==atomic_variable_matrix_dot_product->column()))
	{
		result=atomic_variable_matrix_dot_product->evaluate();
	}

	return (result);
}

EXPORT template<typename Value_type>
bool Function_matrix_dot_product<Value_type>::evaluate_derivative(Scalar&,
	Function_variable_handle,std::list<Function_variable_handle>&)
//******************************************************************************
// LAST MODIFIED : 19 October 2004
//
// DESCRIPTION :
//==============================================================================
{
	return (false);
}

#if !defined (AIX)
template<>
bool Function_matrix_dot_product<Scalar>::evaluate_derivative(
	Scalar& derivative,Function_variable_handle atomic_variable,
	std::list<Function_variable_handle>& atomic_independent_variables);
#endif // !defined (AIX)

EXPORT template<typename Value_type>
bool Function_matrix_dot_product<Value_type>::set_value(
	Function_variable_handle atomic_variable,
	Function_variable_handle atomic_value)
//******************************************************************************
// LAST MODIFIED : 1 December 2004
//
// DESCRIPTION :
//==============================================================================
{
	bool result;
	boost::intrusive_ptr< Function_variable_matrix_dot_product<Value_type> >
		atomic_matrix_variable;
	boost::intrusive_ptr< Function_variable_value_specific<Value_type> >
		value_type;
	Function_handle function;

	result=false;
	if ((atomic_matrix_variable=boost::dynamic_pointer_cast<
		Function_variable_matrix_dot_product<Value_type>,Function_variable>(
		atomic_variable))&&
		equivalent(Function_handle(this),atomic_matrix_variable->function())&&
		atomic_value&&(atomic_value->value())&&(value_type=
		boost::dynamic_pointer_cast<Function_variable_value_specific<Value_type>,
		Function_variable_value>(atomic_value->value())))
	{
		result=value_type->set(this->values(0,0),atomic_value);
	}
	if (result)
	{
		this->set_not_evaluated();
	}
	else
	{
		if (function=variable_1_private->function())
		{
			result=function->set_value(atomic_variable,atomic_value);
		}
		if (!result)
		{
			if (function=variable_2_private->function())
			{
				result=function->set_value(atomic_variable,atomic_value);
			}
		}
	}

	return (result);
}

EXPORT template<typename Value_type>
Function_handle Function_matrix_dot_product<Value_type>::get_value(
	Function_variable_handle atomic_variable)
//******************************************************************************
// LAST MODIFIED : 19 October 2004
//
// DESCRIPTION :
//==============================================================================
{
	Function_handle function,result(0);
	boost::intrusive_ptr< Function_variable_matrix_dot_product<Value_type> >
		atomic_variable_matrix_dot_product;
	ublas::matrix<Value_type,ublas::column_major> result_matrix(1,1);

	if (atomic_variable&&
		equivalent(Function_handle(this),(atomic_variable->function)())&&
		(atomic_variable_matrix_dot_product=boost::dynamic_pointer_cast<
		Function_variable_matrix_dot_product<Value_type>,Function_variable>(
		atomic_variable))&&
		(atomic_variable_matrix_dot_product->get_entry)(result_matrix(0,0)))
	{
		result=Function_handle(new Function_matrix<Value_type>(result_matrix));
	}
	if (!result)
	{
		if (function=variable_1_private->function())
		{
			result=function->get_value(atomic_variable);
		}
		if (!result)
		{
			if (function=variable_2_private->function())
			{
				result=function->get_value(atomic_variable);
			}
		}
	}

	return (result);
}

EXPORT template<typename Value_type>
Function_matrix_dot_product<Value_type>::Function_matrix_dot_product(
	const Function_matrix_dot_product<Value_type>& function_matrix_dot_product):
	Function_matrix<Value_type>(function_matrix_dot_product),
	variable_1_private(function_matrix_dot_product.variable_1_private),
	variable_2_private(function_matrix_dot_product.variable_2_private)
//******************************************************************************
// LAST MODIFIED : 7 December 2004
//
// DESCRIPTION :
// Copy constructor.
//==============================================================================
{
	if (variable_1_private)
	{
		variable_1_private->add_dependent_function(this);
	}
	if (variable_2_private)
	{
		variable_2_private->add_dependent_function(this);
	}
}

EXPORT template<typename Value_type>
Function_matrix_dot_product<Value_type>&
	Function_matrix_dot_product<Value_type>::operator=(
	const Function_matrix_dot_product<Value_type>& function_matrix_dot_product)
//******************************************************************************
// LAST MODIFIED : 7 December 2004
//
// DESCRIPTION :
// Assignment operator.
//==============================================================================
{
	if (function_matrix_dot_product.variable_1_private)
	{
		function_matrix_dot_product.variable_1_private->add_dependent_function(
			this);
	}
	if (variable_1_private)
	{
		variable_1_private->remove_dependent_function(this);
	}
	variable_1_private=function_matrix_dot_product.variable_1_private;
	if (function_matrix_dot_product.variable_2_private)
	{
		function_matrix_dot_product.variable_2_private->add_dependent_function(
			this);
	}
	if (variable_2_private)
	{
		variable_2_private->remove_dependent_function(this);
	}
	variable_2_private=function_matrix_dot_product.variable_2_private;
	this->values=function_matrix_dot_product.values;

	return (*this);
}
