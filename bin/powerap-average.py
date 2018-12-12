from collections import OrderedDict
import argparse, sys, re

def main(argv):
	print "Powercap_config writer started"

	# Parse command line parameters 
	parser = argparse.ArgumentParser()
	parser.add_argument('-i', dest='input')
	parser.add_argument('-o', dest='output')
	args = parser.parse_args()

	# Set myvars based on commnad line parameters 
	if (args.input is None or args.output is None):
		print "Please set a valid input file and output file"
		exit(1)

	count = 0
	totalRuntime = 0
	totalTp = 0
	totalPower = 0
	totalCommits = 0
	totalError = 0

	with open(args.input, 'r') as inputFile:
		for line in inputFile:
			nameValue = re.split(r'\t+', line)
			runtime = ((nameValue[0].split(":"))[1]).lstrip()
			tp = ((nameValue[1].split(":"))[1]).lstrip()
			power = ((nameValue[2].split(":"))[1]).lstrip()
			commits = ((nameValue[3].split(":"))[1]).lstrip()
			error = ((nameValue[4].split(":"))[1]).lstrip()
			totalRuntime = totalRuntime + float(runtime)
			totalTp = totalTp + float(tp)
			totalPower = totalPower + float(power)
			totalCommits = totalCommits + float(commits)
			totalError = totalError + float(error)
			count = count + 1
	
	averageRuntime = totalRuntime/count
	averageTp = totalTp/count
	averagePower = totalPower/count
	averageCommits = totalCommits/count
	averageError = totalError/count

	print count

	with open(args.output, 'w+') as writeFile:
		writeFile.write("Net_runtime: "+str(averageRuntime)+"\tNet_throughput: "+str(averageTp)+"\tNet_power: "+str(averagePower)+"\tNet_commits: "+str(averageCommits)+"\tNet_error: "+str(averageError))

	print "Average written in output file"


if __name__ == "__main__":
   main(sys.argv[1:])