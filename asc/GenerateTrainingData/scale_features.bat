
e:
cd e:\LocalData\ASC-Inclusion\avec2012\featuresFast

%~dp0svm-scale.exe -s scale_params.arousal ipd21_features.arousal.libsvm_raw > ipd21_features.arousal.libsvm_scaled
%~dp0svm-scale.exe -s scale_params.expectancy ipd21_features.expectancy.libsvm_raw > ipd21_features.expectancy.libsvm_scaled
%~dp0svm-scale.exe -s scale_params.power ipd21_features.power.libsvm_raw > ipd21_features.power.libsvm_scaled
%~dp0svm-scale.exe -s scale_params.valence ipd21_features.valence.libsvm_raw > ipd21_features.valence.libsvm_scaled

pause