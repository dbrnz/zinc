#! /usr/bin/perl -w

use strict;

my $group;
my $number_of_fields;
my @field_name;
my %field_number_components;
my %field_coordinate_system;
my %field_coordinate_system_focus;
my %field_components_names;
my %field_components_derivatives;
my %field_components_type_names;
my %field_components_basis;
my %field_components_modification;
my %field_components_mapping;
my %field_components_nodes;
my %field_components_number_of_parameters;
my %field_components_node_indices;
my %field_components_scale_factor_indices;
my $i;
my $j;
my $k;
my $l;
my $values;
my $read_already;
my $node_label_name;
my $element_label_name;
my $element_interpolation_name;
my $scale_factor_list_name;
my %scale_factor_list_lookup;
my $node_list_name;
my $interpolation_name;
my $value_type;
my $node_name;
my $dimension;
my $shape;
my $element_name;
my $face_name;
my $node_list_number;
my %scale_factor_list_numbers;
my $number_of_scale_factor_sets;
my %interpolation;
my $name;
my $new_field;
my $node_index;
my $node_indices;
my $scale_factor_indices;
my $focus;
my @list;
my @node_list_indices;
my @derivative_types;
my @scale_factor_list_indices;
my $derivative;
my $count;
my $derivative_line;
my $basis;
my $index;

my $line_name_offset = 100000;
my $face_name_offset = 200000;

my $in_group;
my $in_element_field;
my $node_field_defined;
my $element_field_declared;
my $element_field_defined;

my $end_match = "(Node:|Shape|Group|#Fields|Element)";

$in_group = 0;
$node_field_defined = 0;
$element_field_declared = 0;
$element_field_defined = 0;

$node_label_name = "NodeTemplatdZ";
$element_label_name = "ElementNameLissZ";
$element_interpolation_name = "ElementInterpolatiomZ";
$scale_factor_list_name = "ScaleFactorSesZ";
$node_list_name = "NodeSesZ";
$interpolation_name = "MappinfZ";

my $time_var = localtime(time());
print <<FIELDML_HEADER;
<fieldml xmlns="http://www.physiome.org.nz/fieldml/0.1#"
         xmlns:fieldml="http://www.physiome.org.nz/fieldml/0.1#">
  <!Generated by ex2fml.pl on $time_var>
FIELDML_HEADER


$read_already = 0;
$_ = <>;
while (defined $_)
  {
	 if (m/Group name\s*:\s*(\w+)/)
	 {
		if ($in_group)
		{
		  if ($group ne $1)
		  {
			 print "  </region>\n";
			 $in_group = 0;
		  }
		}
		$group = $1;

		if (! $in_group)
		{
		   print <<GROUP_HEADER;
 <region name="$group">

GROUP_HEADER
		   $in_group = 1;
		}
	 }

	 elsif (m"#Fields=(\d+)")
	 {
		$number_of_fields = $1;
		for ($i = 0 ; $i < $number_of_fields ; $i++)
		{
		  $_ = <>;
		  if (m"(\d)+\)\s+(\w+), \w+,\s+([^,]+),(\s+focus=\s*([\dE\.\+\-]*),)?\s*#Components=(\d)+")
		  {
			 if ($1 != $i+1)
			 {
				die ("Unable to read field header, field numbers don't match\n$_");
			 }
			 $name = $2;
			 $field_name[$i] = $2;
			 if (!defined $field_number_components{$name})
			 {
				$new_field = 1;
			 }
			 else
			 {
				$new_field = 0;
			 }
			 define_field_parameter(\$field_number_components{$name}, "number of components",
				 $6, $name, $_);
			 define_field_parameter(\$field_coordinate_system{$name}, "coordinate systems",
				 $3, $name, $_);
			 define_field_parameter(\$field_coordinate_system_focus{$name}, "coordinate system focus",
				 $5, $name, $_);
			 for ($j = 0 ; $j < $field_number_components{$name} ; $j++)
			 {
				$_ = <>;
				if (m"([^\.\s][^\.]*)\.\s+Value index=\s*\d+,\s+#Derivatives=\s*(\d+)\s*\(?([\w/,]*)\)?")
				{
				  define_field_parameter(\$field_components_names{$name}[$j],
					 "component names", $1, $name, $_);
				  $field_components_derivatives{$name}[$j] = $2;
				  $field_components_type_names{$name}[$j] = "value,$3";
				}
				elsif (m"([^\.\s][^\.]*)\.\s+([^,]+),\s+([^,]+),\s+([^\.]+)\.")
				{
				  $element_field_declared = 1;
				  define_field_parameter(\$field_components_names{$name}[$j],
					 "component names", $1, $name, $_);
				  $field_components_basis{$name}[$j] = $2;
				  define_field_parameter(\$field_components_modification{$name}[$j],
					 "component modifications", $3, $name, $_);
				  define_field_parameter(\$field_components_mapping{$name}[$j],
					 "component mappings", $4, $name, $_);
				  $_ = <>;
				  if (m"#Nodes=\s*(\d+)")
				  {
					 $field_components_nodes{$name}[$j] = $1;
					 $node_indices = "";
					 $scale_factor_indices = "";
					 for ($k = 0 ; $k < $field_components_nodes{$name}[$j] ; $k++)
					 {
						$_ = <>;
						if (m"(\d+)\.\s+#Values=\s*(\d+)")
						{
						  $node_index = $1;
						}
						else
						{
						  die ("Unable to parse field header, could not read node index\n$_");
						}
						$_ = <>;
						chomp;
						if (m"Value indices:\s+(\d[\d\s]*)")
						{
						  $count = scalar (@list = split(/\s+/, $1));
						  for ($l = 0 ; $l < $count ; $l++)
						  {
							 $node_indices .= "$node_index ";
						  }
						}
						else
						{
						  die ("Unable to parse field header, could not read value indices\n$_");
						}
						$_ = <>;
						chomp;
						if (m"Scale factor indices:\s+(\d[\d\s]*)")
						{
						  $scale_factor_indices .= "$1 ";
						}
						else
						{
						  die ("Unable to parse field header, could not scale factor indices\n$_");
						}
					 }
					 if ((scalar (@list = split(/\s+/, $node_indices))) !=
						  (scalar (@list = split(/\s+/, $scale_factor_indices))))
					 {
						die ("Unable to parse field header, number of node_indices does not match number of scale_factor_indices\nnode indices: $node_indices\nscale_factor_indices $scale_factor_indices");
					 }
					 $field_components_number_of_parameters{$name}[$j] = scalar @list;
					 $field_components_node_indices{$name}[$j] = $node_indices;
					 $field_components_scale_factor_indices{$name}[$j] = $scale_factor_indices;
				  }
				  else
				  {
					 die ("Unable to read field header, number node line didn't parse\n$_");
				  }
				}
				else
				{
				  die ("Unable to read field header, component line didn't parse\n$_");
				}
			 }
		  }
		  else
		  {
			 die ("Unable to read field header, field line didn't parse\n$_");
		  }

		  if ($new_field)
		  {
			 $name = $field_name[$i];
			 if (defined $field_coordinate_system_focus{$name})
			 {
				$focus = "\n		    focus=\"$field_coordinate_system_focus{$name}\"";
			 }
			 else
			 {
				$focus = "";
			 }
			 print <<FIELD_HEADER_1;
	<field name="$field_name[$i]"
		    value_type="real"
		    coordinate_system="$field_coordinate_system{$name}"$focus>
FIELD_HEADER_1

			 for ($j = 0 ; $j < $field_number_components{$name} ; $j++)
			 {
				print <<FIELD_HEADER_2
		<component name="$field_components_names{$name}[$j]"/>
FIELD_HEADER_2
			 }
			 print <<FIELD_HEADER_3;
	</field>

FIELD_HEADER_3
		  }
		}

		$node_field_defined = 0;
		$element_field_defined = 0;

	 }

#Nodes
	 elsif (m/Node:\s*(\d+)/)
	 {
		$node_name = $1;
		$_ = <>;		
		$values = "";
		while ((defined $_) && (! /$end_match/))
		{
		  $values .= $_;
		  $_ = <>;
		}
		$read_already = 1;

#Write the node field if the field is new or different
		if (! $node_field_defined)
		{
		  $node_label_name++;
		  print <<NODE_VALUES_HEADER;
	<labels_template name="$node_label_name">
NODE_VALUES_HEADER
        for ($i = 0 ; $i < $number_of_fields ; $i++)
		  {
			 $name = $field_name[$i];
			 print <<NODE_VALUES_FIELD_1;
		<define_labels label="$field_name[$i]">
NODE_VALUES_FIELD_1

          for ($j = 0 ; $j < $field_number_components{$name} ; $j++)
			 {
				print <<NODE_VALUES_FIELD_2;
			<define_labels label="$field_components_names{$name}[$j]">
NODE_VALUES_FIELD_2

            if (defined $field_components_type_names{$name}[$j])
				{
				  for $value_type (split(',', $field_components_type_names{$name}[$j]))
				  {
					 print <<NODE_VALUES_FIELD_3;
				<define_label label="$value_type"/>
NODE_VALUES_FIELD_3
				  }
				}
				else
				{
				  for ($k = 0 ; $k < $field_components_derivatives{$name}[$j]+1 ; $k++)
				  {
					 print <<NODE_VALUES_FIELD_4;
				<define_label label="value_type_$k"/>
NODE_VALUES_FIELD_4
				  }
				}
				print <<NODE_VALUES_FIELD_5;
			</define_labels>
NODE_VALUES_FIELD_5
			 }
		  print <<NODE_VALUES_FIELD_6;
		</define_labels>
NODE_VALUES_FIELD_6
		  }
		  print <<NODE_VALUES_END;
	</labels_template>

NODE_VALUES_END
         $node_field_defined = 1;
		}

#Write the actual node		
		print <<NODE_1;
	<assign_label label="node_$node_name">
	   <assign_labels labels_template="$node_label_name">
$values		</assign_labels>
	</assign_label>

NODE_1
	 }

#Elements
	 elsif (m/Shape.\s+Dimension=(\d+)(,\s([;\w\(\)\*]*))?/)
	 {
		$dimension = $1;
		if (defined $3)
		{
		  $shape = $3;
		}
		else
		{
		  if ($dimension == 1)
		  {
			 $shape = "line";
		  }
		  elsif ($dimension == 2)
		  {
			 $shape = "line*line";
		  }
		  elsif ($dimension == 3)
		  {
			 $shape = "line*line*line";
		  }
		}
	 }

	 elsif (m"#Nodes=\s*(\d+)")
	 {
		$node_list_number = $1;
		$node_list_name++;
      %interpolation = ();
	 }

	 elsif (m"#Scale factor sets=\s*(\d+)")
	 {
		if (defined $1)
		{
		  $number_of_scale_factor_sets = $1;
		}
		else
		{
		  die ("Could not parse number of scale factors\n$_");
		}
		for ($i = 0 ; $i < $number_of_scale_factor_sets ; $i++)
		{
		   $_ = <>;
		   if (m"([\w\*\.\(\)\;]+),\s*#Scale factors=\s*(\d+)")
			{
		     $scale_factor_list_name++;
		     $scale_factor_list_lookup{$1} = $scale_factor_list_name;
		     $scale_factor_list_numbers{$1} = $2;
         }
         else
         {
				die ("Unable to parse scale factor set definition\n$_");
         }
         %interpolation = ();
      }
	 }

	 elsif (m/Element:\s+(\d+)\s+(\d+)\s+(\d+)/)
	 {
		if ($element_field_declared && ! $element_field_defined)
		{
#Pre define any new interpolation schemes we haven't seen before
        for ($i = 0 ; $i < $number_of_fields ; $i++)
		  {
			 $name = $field_name[$i];
          for ($j = 0 ; $j < $field_number_components{$name} ; $j++)
			 {
				$basis = $field_components_basis{$name}[$j];
				if (! $interpolation{$basis})
				  {
				  $element_label_name++;
				  $interpolation_name++;
				  $interpolation{$field_components_basis{$name}[$j]} = 
					 $interpolation_name;
              @node_list_indices = split(/\s/, $field_components_node_indices{$name}[$j]);
				  @derivative_types = split(/[,\s]/, $field_components_type_names{$name}[$j]);
              @scale_factor_list_indices = split(/\s/, $field_components_scale_factor_indices{$name}[$j]);
				  if (! defined $scale_factor_list_lookup{$basis})
				  {
					 die ("No scale factor set found for basis $basis\n$_");
				  }
				  print <<ELEMENT_LABEL_1;
	<labels_template name="$element_label_name">
      <define_labels label="node_$node_list_indices[0]"> # How do we get the correct node here?
ELEMENT_LABEL_1
              for ($k = 0 ; $k < $field_components_number_of_parameters{$name}[$j] ; $k++)
              {
                if (($k > 0) && ($node_list_indices[$k] eq $node_list_indices[$k-1]))
                {
						 $derivative++;
					 }
					 else
					 {
						 $derivative = 1;
					 }
					 if (defined $derivative_types[$derivative-1])
					 {
						$derivative_line = "$derivative_types[$derivative-1]";
					 }
					 else
					 {
						$derivative_line = "$derivative";
					 }
					 if (($k > 0) && ($node_list_indices[$k] ne  $node_list_indices[$k-1]))
					 {
				      print <<ELEMENT_LABEL_2B;
      </define_labels>
      <define_labels label="node_$node_list_indices[$k]"> # How do we get the correct node here?
ELEMENT_LABEL_2B
					 }
				    print <<ELEMENT_LABEL_2;
         <label name="$derivative_line"/>
ELEMENT_LABEL_2
              }
				 print <<ELEMENT_LABEL_3;
      </define_labels>
	</labels_template>

ELEMENT_LABEL_3
				    print <<INTERPOLATION_1;
   <mapping name="$interpolation_name"
					   basis="$basis"
					   modification="$field_components_modification{$name}[$j]">
      <coefficients>
INTERPOLATION_1
#              for ($k = 0 ; $k < $field_components_number_of_parameters{$name}[$j] ; $k++)
#              {
#					 $index = $k+1;
				    print <<INTERPOLATION_2;
         <product>
            <reference_labels labels_template="$element_label_name"/> #These are in coordinates.x
            <reference_labels labels_template="$scale_factor_list_name"/> #These are in element_*
         </product>
INTERPOLATION_2
#              }
				 print <<INTERPOLATION_3;
       </coefficients>
   </mapping>

INTERPOLATION_3
             }
			  }
		  }

#Now use these in the element fields
		  $element_interpolation_name++;
		  print <<ELEMENT_FIELD_1;
   <element_interpolation name="$element_interpolation_name">
ELEMENT_FIELD_1

        for ($i = 0 ; $i < $number_of_fields ; $i++)
		  {
			 $name = $field_name[$i];
		  print <<ELEMENT_FIELD_2;
      <default_label label="$field_name[$i]">
ELEMENT_FIELD_2
          for ($j = 0 ; $j < $field_number_components{$name} ; $j++)
			 {
				print <<ELEMENT_FIELD_3;
         <default_label label="$field_components_names{$name}[$j]">
            <mapping_ref ref="$interpolation{$field_components_basis{$name}[$j]}"/> # The node numbers are changing in here so the elements are in the wrong order
         </default_label>
ELEMENT_FIELD_3
			 }
		  print <<ELEMENT_FIELD_5;
      </default_label>
ELEMENT_FIELD_5
		  }
		  print <<ELEMENT_FIELD_6;
   </element_interpolation>

ELEMENT_FIELD_6

		  $element_field_defined = 1;
		}

		if ($dimension == 1)
		{
		  $element_name = $3 + $line_name_offset;
		}
		elsif ($dimension == 2)
		{
		  $element_name = $2 + $face_name_offset;
		}
		elsif ($dimension == 3)
		{
		  $element_name = $1;
		}
		else
		{
		  die ("Unable to read element, dimension is $dimension");
		}
	
 		print <<ELEMENT_HEADER;
	<element	name="$element_name"
            shape="$shape">
     <assign_label label="element_$element_name"> # Seems annoying to redefine the same name
ELEMENT_HEADER

      $in_element_field = 0;
		$_ = <>;
		while ((defined $_) && (! m/$end_match/))
		{
		  if (m/Faces:/)
		  {
			 print <<ELEMENT_FACES_1;
		<faces>
ELEMENT_FACES_1
			 $_ = <>;
			 while ((defined $_) && (! m/(Scale|Nodes)/) &&
				 (! m/$end_match/))
			 {
				if (m/(\d+)\s+(\d+)\s+(\d+)/)
				{
				  if ($dimension == 3)
				  {
					 $face_name = $2 + $face_name_offset;
				  }
				  elsif ($dimension == 2)
				  {
					 $face_name = $3 + $line_name_offset;
				  }
				  else
				  {
					 die ("Unable to read faces, dimension is $dimension");
				  }
				  print " $face_name";
				}
				else
				{
				  die ("Unable to read face, line didn't parse\n$_");
				}
				$_ = <>;
			 }

			 print <<ELEMENT_FACES_3;

		</faces>
ELEMENT_FACES_3
		  }
 		  elsif (m/Nodes:/)
		  {
			 print <<ELEMENT_NODES_2;
		<element_interpolation_ref ref="$element_interpolation_name"/>
		<assign_label label="$node_list_name">
ELEMENT_NODES_2
          $_ = <>;
			 while ((defined $_) && (! m/(Scale|Nodes)/) &&
				 (! m/$end_match/))
			 {
				print $_;
				$_ = <>;
			 }

			 print <<ELEMENT_NODES_3;
		</label>
ELEMENT_NODES_3
		  }
 		  elsif (m/Scale factors:/)
		  {
			 print <<ELEMENT_SCALE_2;
		<assign_label name="$scale_factor_list_name">
ELEMENT_SCALE_2

          $_ = <>;
			 while ((defined $_) && (! m/(Scale|Nodes)/) &&
				 (! m/$end_match/))
			 {
				print $_;
				$_ = <>;
			 }

			 print <<ELEMENT_SCALE_3;
		</assign_label>
ELEMENT_SCALE_3
		  }
		  else
		  {
			 $_ = <>;
		  }
		}


 		print <<ELEMENT_END;
     </assign_label>
	</element>

ELEMENT_END
		$read_already = 1;
	 }

	 if ($read_already)
		{
		  $read_already = 0;
		}
	 else
		{
		  $_ = <>;
		}
  }

if ($in_group)
  {
	 print <<END_GROUP;
  </region>
END_GROUP
  }

print "</fieldml>\n";


sub define_field_parameter
{
  my $parameter_ref = shift;
  my $parameter_name = shift;
  my $new_value = shift;
  my $name = shift;
  my $line = shift;

  if (defined $$parameter_ref)
  {
	 if ($$parameter_ref ne $new_value)
	 {
		die ("Redefinition of field $name is inconsistent, $parameter_name don't match\n$line");
	 }
  }
  else
  {
	 $$parameter_ref = $new_value;
  }
}
