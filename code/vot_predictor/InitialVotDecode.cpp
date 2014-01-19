/************************************************************************
 Copyright (c) 2010 Joseph Keshet
 
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are
 met: Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 The name of the author may not be used to endorse or promote products
 derived from this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.
 ************************************************************************/

/************************************************************************
 Project:  Initial VOT Detection
 Module:   Main entry point
 Purpose:  Predict the locationof initiat VOT (decoding)
 Date:     26 Feb., 2010
 Programmer: Joseph Keshet
 
 **************************** INCLUDE FILES *****************************/
#include <iostream>
#include <fstream>
#include <map>
#include <cmdline/cmd_line.h>
#include "Classifier.h"
#include "Dataset.h"

using namespace std;

static int loss_resolutions[] = {2,5,10,15,20,25,50};

/************************************************************************
 Function:     main
 
 Description:  Main entry point
 Inputs:       int argc, char *argv[] - main input params
 Output:       int - EXIT_SUCCESS or EXIT_FAILURE
 Comments:     none.
 ***********************************************************************/
int main(int argc, char **argv)
{
	// Parse command line
	int min_vot_length;
	int max_vot_length;
	int max_onset_time;
	string instances_filelist;
	string labels_filename;
	string classifier_filename;
	string output_predictions_filename;
	string ignore_features_str;
	bool pos_only;
	string kernel_expansion_name;
	double sigma;
	
	learning::cmd_line cmdline;
	cmdline.info("Initial VOT detection - Passive Aggressive decoding");
	cmdline.add("-min_vot_length", "min. phoneme duration in msec [15]", &min_vot_length, 15);
	cmdline.add("-max_vot_length", "max. phoneme duration in msec [200]", &max_vot_length, 200);
	cmdline.add("-max_onset", "min. phoneme duration in msec [150]", &max_onset_time, 150);
	cmdline.add("-output_predictions", "predictions and their confidences are written to the given file",
							&output_predictions_filename, "");
	cmdline.add("-ignore_features", "ignore the following features. E.g., \"3,7,19\".", &ignore_features_str, "");
	cmdline.add("-pos_only", "Assume only positive VOTs", &pos_only, false);
  cmdline.add("-kernel_expansion", "use kernel expansion of type 'poly2' or 'rbf2'", &kernel_expansion_name, "");
  cmdline.add("-sigma", "if kernel is rbf2 or rbf3 this is the sigma", &sigma, 1.0);
	cmdline.add_master_option("instances_filelist", &instances_filelist);
	cmdline.add_master_option("labels_filename[can be `null` for no labels]", &labels_filename);
	cmdline.add_master_option("classifier_filename", &classifier_filename);
	int rc = cmdline.parse(argc, argv);
	if (rc < 3) {
		cmdline.print_help();
		return EXIT_FAILURE;
	}
	
	// Initiate classifier
	Classifier classifier(min_vot_length, max_vot_length, max_onset_time, 0.0, 0.0, 0.0, 0.0, kernel_expansion_name, sigma);
	classifier.load(classifier_filename);
	if (ignore_features_str != "") {
		cout << "Info: ignoring features " << ignore_features_str << "." << endl;
		classifier.ignore_features(ignore_features_str);
	}
	
	// begining of the training set
	Dataset test_dataset(instances_filelist, labels_filename);
	
	int num_boundaries = 0;
	int num_vots = 0;
	int num_pos = 0;
	int num_neg = 0;
	int neg_mislabeled_pos = 0;
	int pos_mislabeled_neg = 0;
	
	int cumulative_loss = 0;
	int *cum_loss_less_than = new int[sizeof(loss_resolutions)/sizeof(int)];
	
	int *cum_vot_loss_less_than = new int[sizeof(loss_resolutions)/sizeof(int)];
	int cumulative_vot_loss = 0;
	
	int *cum_corr_loss_less_than = new int[sizeof(loss_resolutions)/sizeof(int)];
	int cumulative_corr_loss = 0;
	
	for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++) {
		cum_loss_less_than[j] = 0;
		cum_vot_loss_less_than[j] = 0;
		cum_corr_loss_less_than[j] = 0;
	}
	
	double rms_onset_loss = 0;
	
	ofstream output_predictions_ofs;
	if (output_predictions_filename != "") {
		output_predictions_ofs.open(output_predictions_filename.c_str());
		if (!output_predictions_ofs.good()) {
			cerr << "Error: unable to open " << output_predictions_filename << "for writing." << endl;
		}
	}
	
	// Run over all dataset
	for (uint i=0; i <  test_dataset.size(); i++) {
		
		SpeechUtterance x;
		VotLocation y;
		VotLocation y_hat;
		
		cout << "==================================================================================" << endl;
		
		// read next example for dataset
		test_dataset.read(x, y);
		
		// predict label
		double confidence = classifier.predict(x, y_hat, pos_only);
		
		if (output_predictions_filename != "" && output_predictions_ofs.good())
			output_predictions_ofs << confidence << " " << y_hat.burst << " " << y_hat.voice << std::endl;
		
		// calculate the error
		if (test_dataset.labels_given()) {
			int onset_loss = abs(y.burst - y_hat.burst);
			for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++)
				if ( onset_loss <= loss_resolutions[j] ) cum_loss_less_than[j]++;
			
			
			rms_onset_loss += onset_loss*onset_loss;
			
			int offset_loss = abs(y.voice - y_hat.voice);
			for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++)
				if ( offset_loss <= loss_resolutions[j] ) cum_loss_less_than[j]++;
			
			cout << "burst onset err: " << y_hat.burst - y.burst << endl;
			cout << "voice onset err: " << y_hat.voice - y.voice << endl;
			
			int vot_loss = abs(y.voice-y.burst-(y_hat.voice-y_hat.burst));
			for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++)
				if ( vot_loss <= loss_resolutions[j] ) cum_vot_loss_less_than[j]++;
			
			cumulative_loss += onset_loss + offset_loss;
			num_boundaries += 2;
			
			cumulative_vot_loss += vot_loss;
			num_vots += 1;
			
			if(y.burst < y.voice) { //positive VOT example
				num_pos++;
				if(y_hat.burst < y_hat.voice) { //correctly predicited pos
					for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++)
						if ( vot_loss <= loss_resolutions[j] ) cum_corr_loss_less_than[j]++;
					cumulative_corr_loss += vot_loss;
				} else { //incorrectly predicted neg
					pos_mislabeled_neg++;
				}
			} else { //negative VOT example
				num_neg++;
				if(y_hat.burst > y_hat.voice) { //correctly predicted neg
					for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++)
						if ( vot_loss <= loss_resolutions[j] ) cum_corr_loss_less_than[j]++;
					cumulative_corr_loss += vot_loss;
				} else { //incorrectly predicted pos
					neg_mislabeled_pos++;
				}
			}
			
			// labeled and predicted VOT
			cout << "Labeled VOT: " << y.burst << " " << y.voice << endl;
			cout << "Predicted VOT: " << y_hat.burst << " "
			<< y_hat.voice << " conf: " << confidence << endl;
			
			// boundaries t<= table
			cout << "Cum loss = " << cumulative_loss/double(num_boundaries) << endl;
			for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++) {
				cout << "% Boundaries (t <= " << loss_resolutions[j] << "ms) = "
				<< 100.0*cum_loss_less_than[j]/double(num_boundaries) << "\n";
			}
			
			cout << endl;
			
			// VOT t<= table
			cout << "Cum VOT loss = " << cumulative_vot_loss/double(num_vots) << endl;
			for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++) {
				cout << "% VOT error (t <= " << loss_resolutions[j] << "ms) = "
				<< 100.0*cum_vot_loss_less_than[j]/double(num_vots) << "\n";
			}
			
			cout << endl;
			
			// percent misclassified
			int num_misclassified = neg_mislabeled_pos + pos_mislabeled_neg;
			int num_corr = num_vots - num_misclassified;
			cout << "Total num misclassified = "
			<< double(num_misclassified)/double(num_vots) << endl;
			cout << "Num pos misclassified as neg = "
			<< double(pos_mislabeled_neg)/double(num_pos) << endl;
			cout << "Num neg misclassified as pos = "
			<< double(neg_mislabeled_pos)/double(num_neg) << endl;
			
			// VOT t<= table for correctly classified data only
			cout << "Cum VOT loss on correctly classified data = "
			<< cumulative_corr_loss/double(num_corr)
			<< endl;
			for (uint j=0; j < sizeof(loss_resolutions)/sizeof(int); j++) {
				cout << "% corr VOT error (t <= " << loss_resolutions[j] << "ms) = "
				<< 100.0*cum_corr_loss_less_than[j]/double(num_corr) << "\n";
			}
			
			cout << endl;
		}
	}
	
	rms_onset_loss /= double(num_vots);
	rms_onset_loss = sqrt(num_vots);
	
	cout << "rms onset loss: " << rms_onset_loss << endl;
	
	if (output_predictions_filename != "" && output_predictions_ofs.good())
		output_predictions_ofs.close();
	
	delete [] cum_loss_less_than ;
	cout << "Done." << endl;  
	
	return EXIT_SUCCESS;
	
}

// ------------------------------- EOF -----------------------------//
