#!/usr/bin/env python
"""This script converts an XML description of test cases into an Expect
script.

Copyright (C) University of Guelph, 2003-2008

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version."""

__author__ = "Neil Harvey <nharve01@uoguelph.ca>"
__date__ = "December 2003"

import sys
import xml.dom.minidom
import sqlite3
import os



def getText (element):
	"""Returns the contents of all text children of the given element,
	concatenated, as a string."""
	text = ""
	for node in element.childNodes:
		if node.nodeType == node.TEXT_NODE:
			text += node.data
	return text.strip()



def getUnitNames (parameterFileName, populationFileName):
	"""Return an array of strings, giving the names of units in the population
	file in the order they appear."""
	# Check for a database file first
	try:
		uri = 'file:' + parameterFileName + '_' + populationFileName + '.db?mode=rw'
		conn = sqlite3.connect(uri, uri=True)
		names = [row[0] for row in conn.execute('SELECT unit_id FROM ScenarioCreator_unit')]
	except sqlite3.OperationalError:
	    # Try for an XML population file
		try:
			doc = xml.dom.minidom.parse(os.path.join(os.pardir, populationFileName + '.xml'))
			names = [getText(node) for node in doc.getElementsByTagName ("id")]
		except IOError:
			names = None
	return names



def rowToStates (row, state_code):
	"""Returns an array of state codes given a row of <td> elements from the
	input file.  Accepts 1-letter codes (e.g. "S"), long names (e.g.
	"Susceptible"), and strings of 1-letter codes (e.g. "SLCND")."""
	states = []
	for cell in row.getElementsByTagName ("td"):
		state = getText (cell)
		if state in state_code.values():
			states.append (state) # this is a valid 1-letter code
		elif state in state_code:
			states.append (state_code[state]) # convert long name to 1-letter code
		else:
			# Break into chars, see if each is a valid 1-letter code
			nospaces = state.replace(' ','')
			if all([char in state_code.values() for char in nospaces]):
				states += [char for char in nospaces]
			else:
				raise ValueError('"%s" is not a recognized state' % state)
	# end of loop over <td> elements
	return states



def main ():
	doc = xml.dom.minidom.parse (sys.stdin)
	# Boilerplate at the top of the file.
	print(r"""set timeout 3
#
# expectations that clean up in case of error. Note that `$test' is
# a purely local variable.
#
# The first of these is used to match any bad responses, and resynchronise
# things by finding a prompt. The second is a timeout error, and shouldn't
# ever be triggered.
#
expect_after {
	-re "\[^\n\r\]*$prompt$" {
		fail "$test (bad match)"
		if { $verbose > 0 } {
			regexp ".*\r\n(\[^\r\n\]+)(\[\r\n\])+$prompt$" \
						$expect_out(buffer) "" output
			send_user "\tUnmatched output: \"$output\"\n"
		}
	}
	timeout {
		fail "$test (timeout)"
	}
}""")

	state_code = {
	  'Susceptible': 'S', # map long names to short ones
	  'Latent': 'L',
	  'InfectiousSubclinical': 'B',
	  'InfectiousClinical': 'C',
	  'NaturallyImmune': 'N',
	  'VaccineImmune': 'V',
	  'Destroyed': 'D'
	}

	# Print the deterministic unit-state tests.
	tests = doc.getElementsByTagName ("deterministic-test")
	for test in tests:
		description = getText (test.getElementsByTagName ("description")[0])
		print('\n\n\n#')
		for line in description.split('\n'):
			print('# %s' % line.strip())
		print('#')

		parameters = getText (test.getElementsByTagName ("parameter-description")[0])
		for line in parameters.split('\n'):
			print('# %s' % line.strip())
		print('#')

		category = getText (test.getElementsByTagName ("category")[0])
		parameterFileName = getText (test.getElementsByTagName ("parameter-file")[0])
		populationFileName = getText (test.getElementsByTagName ("population-file")[0])
		unitNames = getUnitNames (parameterFileName, populationFileName)
		print('set scenario "test/module.%s/%s_%s.db"' % (category, parameterFileName, populationFileName))

		table = test.getElementsByTagName ("output")[0]
		rows = [rowToStates(row, state_code)
		        for row in table.getElementsByTagName ("tr")]
		print('set states [dict create \\')
		for i in range(len(unitNames)):
			print('  "%s" {' % unitNames[i], end=' ')
			print(' '.join([row[i] for row in rows]), end=' ')
			print('} \\')
		print(']')

		shortName = getText (test.getElementsByTagName ("short-name")[0])
		print('progress_test $scenario $states "%s"' % shortName)

	# Print the stochastic herd-state tests.
	tests = doc.getElementsByTagName ("stochastic-test")
	for test in tests:
		description = getText (test.getElementsByTagName ("description")[0])
		print('\n\n\n#')
		for line in description.split('\n'):
			print('# %s' % line.strip())
		print('#')

		parameters = getText (test.getElementsByTagName ("parameter-description")[0])
		for line in parameters.split('\n'):
			print('# %s' % line.strip())
		print('#')

		category = getText (test.getElementsByTagName ("category")[0])
		parameterFileName = getText (test.getElementsByTagName ("parameter-file")[0])
		populationFileName = getText (test.getElementsByTagName ("population-file")[0])
		unitNames = getUnitNames (parameterFileName, populationFileName)
		print('set scenario "test/module.%s/%s_%s.db"' % (category, parameterFileName, populationFileName))

		tables = test.getElementsByTagName ("output")
		print('set states {')
		for table in tables:
			print('  {', table.getAttribute ("probability"))
			rows = [rowToStates(row, state_code)
			        for row in table.getElementsByTagName ("tr")]
			print('    {')
			for i in range(len(unitNames)):
				print('      "%s" {' % unitNames[i], end=' ')
				print(' '.join([row[i] for row in rows]), end=' ')
				print('}')
			print('    }')
			print('  }')
		print('}')

		shortName = getText (test.getElementsByTagName ("short-name")[0])
		print('stochastic_progress_test $scenario $states "%s"' % shortName)

	# Print the deterministic output variable tests.
	tests = doc.getElementsByTagName ("variable-test")
	for test in tests:
		description = getText (test.getElementsByTagName ("description")[0])
		print('\n\n\n#')
		for line in description.split('\n'):
			print('# %s' % line.strip())
		print('#')

		parameters = getText (test.getElementsByTagName ("parameter-description")[0])
		for line in parameters.split('\n'):
			print('# %s' % line.strip())
		print('#')

		category = getText (test.getElementsByTagName ("category")[0])
		parameterFileName = getText (test.getElementsByTagName ("parameter-file")[0])
		populationFileName = getText (test.getElementsByTagName ("population-file")[0])
		print('set scenario "test/module.%s/%s_%s.db"' % (category, parameterFileName, populationFileName))

		table = test.getElementsByTagName ("output")[0]
		rows = table.getElementsByTagName ("tr")
		varnames = [getText (cell) for cell in rows[0].getElementsByTagName ("td")]
		del rows[0]
		print('set values {')
		for i in range (len (varnames)):
			print('  { "%s"' % varnames[i], end=' ')
			for row in rows:
				cell = row.getElementsByTagName ("td")[i]
				print(getText (cell) or '""', end=' ')
			print('}')
		print('}')

		shortName = getText (test.getElementsByTagName ("short-name")[0])
		print('variable_test $scenario $values "%s"' % shortName)

	# Print the stochastic output variable tests.
	tests = doc.getElementsByTagName ("stochastic-variable-test")
	for test in tests:
		description = getText (test.getElementsByTagName ("description")[0])
		print('\n\n\n#')
		for line in description.split('\n'):
			print('# %s' % line.strip())
		print('#')

		parameters = getText (test.getElementsByTagName ("parameter-description")[0])
		for line in parameters.split('\n'):
			print('# %s' % line.strip())
		print('#')

		category = getText (test.getElementsByTagName ("category")[0])
		parameterFileName = getText (test.getElementsByTagName ("parameter-file")[0])
		populationFileName = getText (test.getElementsByTagName ("population-file")[0])
		print('set scenario "test/module.%s/%s_%s.db"' % (category, parameterFileName, populationFileName))
		tables = test.getElementsByTagName ("output")

		print('set values {')
		for table in tables:
			print('  {', table.getAttribute ("probability"))
			rows = table.getElementsByTagName ("tr")
			varnames = [getText (cell) for cell in rows[0].getElementsByTagName ("td")]
			del rows[0]
			for i in range (len (varnames)):
				print('    { "%s"' % varnames[i], end=' ')
				for row in rows:
					cell = row.getElementsByTagName ("td")[i]
					print(getText (cell) or '""', end=' ')
				print('}')
			print('  }')
		print('}')

		shortName = getText (test.getElementsByTagName ("short-name")[0])
		print('stochastic_variable_test $scenario $values "%s"' % shortName)



if __name__ == "__main__":
	main()
