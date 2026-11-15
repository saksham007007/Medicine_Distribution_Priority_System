// Demonstration medicine catalogue: generic | category | Rx(1/0) | base unit price INR | Form=strengths;Form=strengths
// Generic names, categories and strengths follow commonly marketed formulations, but brand names, manufacturers,
// prices and stock generated from this table are SYNTHETIC demonstration data. Not for clinical use.
#pragma once
static const char* CATALOGUE[] = {
R"CAT(
Amoxicillin|Antibiotics|1|6|Capsule=250mg,500mg;Syrup=125mg/5ml,250mg/5ml
Amoxicillin + Clavulanic Acid|Antibiotics|1|14|Tablet=375mg,625mg,1000mg;Syrup=228.5mg/5ml
Azithromycin|Antibiotics|1|18|Tablet=250mg,500mg;Syrup=200mg/5ml
Ciprofloxacin|Antibiotics|1|5|Tablet=250mg,500mg,750mg
Levofloxacin|Antibiotics|1|10|Tablet=250mg,500mg,750mg
Ofloxacin|Antibiotics|1|6|Tablet=200mg,400mg
Norfloxacin|Antibiotics|1|4|Tablet=400mg
Cefixime|Antibiotics|1|14|Tablet=100mg,200mg,400mg;Syrup=50mg/5ml,100mg/5ml
Cefuroxime|Antibiotics|1|25|Tablet=250mg,500mg
Cefpodoxime|Antibiotics|1|20|Tablet=100mg,200mg
Cefalexin|Antibiotics|1|7|Capsule=250mg,500mg
Cloxacillin|Antibiotics|1|8|Capsule=250mg,500mg
Doxycycline|Antibiotics|1|5|Capsule=100mg
Clarithromycin|Antibiotics|1|22|Tablet=250mg,500mg
Erythromycin|Antibiotics|1|8|Tablet=250mg,500mg
Metronidazole|Antibiotics|1|2|Tablet=200mg,400mg;Suspension=200mg/5ml
Tinidazole|Antibiotics|1|4|Tablet=500mg
Co-trimoxazole|Antibiotics|1|3|Tablet=480mg,960mg
Nitrofurantoin|Antibiotics|1|7|Capsule=50mg,100mg
Linezolid|Antibiotics|1|45|Tablet=600mg
Clindamycin|Antibiotics|1|16|Capsule=150mg,300mg
Gentamicin|Antibiotics|1|3|Injection=40mg/ml
Ceftriaxone|Antibiotics|1|7|Injection=250mg,500mg,1g
Amikacin|Antibiotics|1|6|Injection=250mg,500mg
Piperacillin + Tazobactam|Antibiotics|1|55|Injection=2.25g,4.5g
)CAT",
R"CAT(
Paracetamol|Antipyretics|0|1.2|Tablet=250mg,500mg,650mg;Syrup=125mg/5ml,250mg/5ml;Suspension=100mg/ml
Ibuprofen|Analgesics|0|1.5|Tablet=200mg,400mg,600mg;Suspension=100mg/5ml
Diclofenac|Analgesics|1|1.8|Tablet=50mg,100mg;Injection=75mg/3ml;Gel=1%
Naproxen|Analgesics|1|3|Tablet=250mg,500mg
Aceclofenac|Analgesics|1|3|Tablet=100mg
Etoricoxib|Analgesics|1|9|Tablet=60mg,90mg,120mg
Tramadol|Analgesics|1|5|Capsule=50mg
Mefenamic Acid|Analgesics|1|2.5|Tablet=250mg,500mg
Ketorolac|Analgesics|1|4|Tablet=10mg;Injection=30mg/ml
Piroxicam|Analgesics|1|3|Capsule=10mg,20mg
Aluminium Hydroxide + Magnesium Hydroxide|Antacids|0|1.5|Tablet=250mg/250mg;Suspension=200mg/200mg
Calcium Carbonate|Antacids|0|1.2|Tablet=500mg,1250mg
Sodium Bicarbonate|Antacids|0|0.8|Tablet=500mg
Cetirizine|Antihistamines|0|1.2|Tablet=5mg,10mg;Syrup=5mg/5ml
Levocetirizine|Antihistamines|0|2.5|Tablet=2.5mg,5mg;Syrup=2.5mg/5ml
Loratadine|Antihistamines|0|2|Tablet=10mg;Syrup=5mg/5ml
Fexofenadine|Antihistamines|0|6|Tablet=60mg,120mg,180mg
Desloratadine|Antihistamines|0|5|Tablet=5mg
Chlorpheniramine|Antihistamines|0|0.5|Tablet=4mg;Syrup=2mg/5ml
Hydroxyzine|Antihistamines|1|3|Tablet=10mg,25mg
Promethazine|Antihistamines|1|2|Tablet=10mg,25mg;Syrup=5mg/5ml
Diphenhydramine|Antihistamines|0|1|Syrup=12.5mg/5ml
Bilastine|Antihistamines|0|12|Tablet=20mg
)CAT",
R"CAT(
Metformin|Antidiabetics|1|1.2|Tablet=500mg,850mg,1000mg;Extended-release Tablet=500mg,1000mg
Glimepiride|Antidiabetics|1|3|Tablet=1mg,2mg,3mg,4mg
Gliclazide|Antidiabetics|1|4|Tablet=40mg,80mg;Extended-release Tablet=30mg,60mg
Glipizide|Antidiabetics|1|2.5|Tablet=2.5mg,5mg
Sitagliptin|Antidiabetics|1|28|Tablet=25mg,50mg,100mg
Vildagliptin|Antidiabetics|1|22|Tablet=50mg
Linagliptin|Antidiabetics|1|45|Tablet=5mg
Teneligliptin|Antidiabetics|1|14|Tablet=20mg
Pioglitazone|Antidiabetics|1|4|Tablet=15mg,30mg,45mg
Dapagliflozin|Antidiabetics|1|30|Tablet=5mg,10mg
Empagliflozin|Antidiabetics|1|40|Tablet=10mg,25mg
Acarbose|Antidiabetics|1|8|Tablet=25mg,50mg,100mg
Voglibose|Antidiabetics|1|9|Tablet=0.2mg,0.3mg
Repaglinide|Antidiabetics|1|6|Tablet=0.5mg,1mg,2mg
Insulin Human Regular|Antidiabetics|1|30|Injection=100IU/ml
Insulin Glargine|Antidiabetics|1|60|Injection=100IU/ml
Insulin Aspart|Antidiabetics|1|55|Injection=100IU/ml
Amlodipine|Cardiovascular|1|1.5|Tablet=2.5mg,5mg,10mg
Atenolol|Cardiovascular|1|1.5|Tablet=25mg,50mg,100mg
Metoprolol|Cardiovascular|1|3|Tablet=25mg,50mg,100mg
Bisoprolol|Cardiovascular|1|5|Tablet=2.5mg,5mg,10mg
Carvedilol|Cardiovascular|1|4|Tablet=3.125mg,6.25mg,12.5mg,25mg
Propranolol|Cardiovascular|1|1.5|Tablet=10mg,20mg,40mg,80mg
Losartan|Cardiovascular|1|3|Tablet=25mg,50mg,100mg
Telmisartan|Cardiovascular|1|5|Tablet=20mg,40mg,80mg
Olmesartan|Cardiovascular|1|8|Tablet=20mg,40mg
Valsartan|Cardiovascular|1|8|Tablet=40mg,80mg,160mg
Ramipril|Cardiovascular|1|5|Tablet=1.25mg,2.5mg,5mg,10mg
Enalapril|Cardiovascular|1|2|Tablet=2.5mg,5mg,10mg
Lisinopril|Cardiovascular|1|3|Tablet=2.5mg,5mg,10mg,20mg
Hydrochlorothiazide|Cardiovascular|1|1.5|Tablet=12.5mg,25mg
Chlorthalidone|Cardiovascular|1|3|Tablet=6.25mg,12.5mg,25mg
Furosemide|Cardiovascular|1|1.5|Tablet=20mg,40mg;Injection=10mg/ml
Spironolactone|Cardiovascular|1|4|Tablet=25mg,50mg,100mg
Torsemide|Cardiovascular|1|5|Tablet=10mg,20mg
Atorvastatin|Cardiovascular|1|5|Tablet=10mg,20mg,40mg,80mg
Rosuvastatin|Cardiovascular|1|8|Tablet=5mg,10mg,20mg,40mg
Simvastatin|Cardiovascular|1|4|Tablet=10mg,20mg,40mg
Fenofibrate|Cardiovascular|1|9|Capsule=160mg,200mg
Clopidogrel|Cardiovascular|1|4|Tablet=75mg
Aspirin|Cardiovascular|1|1|Tablet=75mg,150mg
Ticagrelor|Cardiovascular|1|35|Tablet=60mg,90mg
Warfarin|Cardiovascular|1|3|Tablet=1mg,2mg,5mg
Rivaroxaban|Cardiovascular|1|60|Tablet=10mg,15mg,20mg
Apixaban|Cardiovascular|1|45|Tablet=2.5mg,5mg
Isosorbide Mononitrate|Cardiovascular|1|4|Tablet=10mg,20mg,30mg
Digoxin|Cardiovascular|1|1.5|Tablet=0.25mg
Amiodarone|Cardiovascular|1|12|Tablet=100mg,200mg
Diltiazem|Cardiovascular|1|4|Tablet=30mg,60mg,90mg
Verapamil|Cardiovascular|1|4|Tablet=40mg,80mg
Nifedipine|Cardiovascular|1|3|Extended-release Tablet=10mg,20mg,30mg
Ivabradine|Cardiovascular|1|20|Tablet=5mg,7.5mg
Sacubitril + Valsartan|Cardiovascular|1|55|Tablet=24mg/26mg,49mg/51mg,97mg/103mg
)CAT",
R"CAT(
Clotrimazole|Dermatological|0|3|Cream=1%;Solution=1%
Betamethasone|Dermatological|1|5|Cream=0.05%,0.1%
Mometasone|Dermatological|1|7|Cream=0.1%
Hydrocortisone|Dermatological|1|3|Cream=1%
Mupirocin|Dermatological|1|9|Ointment=2%
Fusidic Acid|Dermatological|1|8|Cream=2%
Ketoconazole|Dermatological|0|5|Cream=2%;Shampoo=2%
Terbinafine|Dermatological|0|7|Cream=1%;Tablet=250mg
Luliconazole|Dermatological|0|10|Cream=1%
Miconazole|Dermatological|0|4|Cream=2%
Permethrin|Dermatological|1|8|Cream=5%;Lotion=1%
Adapalene|Dermatological|1|9|Gel=0.1%,0.3%
Benzoyl Peroxide|Dermatological|0|6|Gel=2.5%,5%
Tretinoin|Dermatological|1|8|Cream=0.025%,0.05%
Clindamycin Phosphate|Dermatological|1|9|Gel=1%
Silver Sulfadiazine|Dermatological|1|6|Cream=1%
Tacrolimus|Dermatological|1|14|Ointment=0.03%,0.1%
Salicylic Acid|Dermatological|0|4|Ointment=3%,6%
Minoxidil|Dermatological|0|10|Solution=2%,5%
Povidone Iodine|Dermatological|0|3|Ointment=5%;Solution=5%,10%
Calamine|Dermatological|0|2|Lotion=8%
Vitamin C|Vitamins and Supplements|0|1.5|Tablet=500mg
Cholecalciferol (Vitamin D3)|Vitamins and Supplements|0|12|Capsule=60000IU;Tablet=1000IU
Calcium + Vitamin D3|Vitamins and Supplements|0|4|Tablet=500mg/250IU
Methylcobalamin (Vitamin B12)|Vitamins and Supplements|0|5|Tablet=500mcg,1500mcg;Injection=1500mcg/ml
Folic Acid|Vitamins and Supplements|0|0.6|Tablet=5mg
Ferrous Ascorbate + Folic Acid|Vitamins and Supplements|0|4|Tablet=100mg/1.5mg
Ferrous Fumarate|Vitamins and Supplements|0|3|Tablet=200mg
Zinc Sulfate|Vitamins and Supplements|0|2.5|Tablet=20mg
Vitamin B-Complex|Vitamins and Supplements|0|1.2|Tablet=standard
Omega-3 Fatty Acids|Vitamins and Supplements|0|10|Capsule=1000mg
Vitamin E|Vitamins and Supplements|0|3|Capsule=200mg,400mg
Pyridoxine (Vitamin B6)|Vitamins and Supplements|0|1.5|Tablet=10mg,40mg
Thiamine (Vitamin B1)|Vitamins and Supplements|0|1.2|Tablet=100mg
Multivitamin + Multimineral|Vitamins and Supplements|0|6|Capsule=standard;Syrup=standard
Vitamin K1|Vitamins and Supplements|1|18|Injection=10mg/ml
)CAT",
R"CAT(
Salbutamol|Respiratory|1|1|Tablet=2mg,4mg;Syrup=2mg/5ml;Inhaler=100mcg/dose
Levosalbutamol|Respiratory|1|3|Tablet=1mg;Syrup=1mg/5ml;Inhaler=50mcg/dose
Budesonide|Respiratory|1|4|Inhaler=100mcg/dose,200mcg/dose,400mcg/dose;Respules=0.5mg/2ml
Budesonide + Formoterol|Respiratory|1|9|Inhaler=100mcg/6mcg,200mcg/6mcg,400mcg/6mcg
Fluticasone|Respiratory|1|7|Inhaler=125mcg/dose,250mcg/dose
Salmeterol + Fluticasone|Respiratory|1|10|Inhaler=25mcg/125mcg,25mcg/250mcg
Ipratropium|Respiratory|1|4|Inhaler=20mcg/dose;Respules=500mcg/2ml
Tiotropium|Respiratory|1|12|Inhaler=9mcg/dose,18mcg/dose
Montelukast|Respiratory|1|8|Tablet=4mg,5mg,10mg
Theophylline|Respiratory|1|2|Extended-release Tablet=100mg,200mg,300mg
Doxofylline|Respiratory|1|7|Tablet=400mg
Ambroxol|Respiratory|0|2|Tablet=30mg;Syrup=15mg/5ml
Bromhexine|Respiratory|0|1.5|Tablet=8mg;Syrup=4mg/5ml
Guaifenesin|Respiratory|0|2|Syrup=100mg/5ml
Dextromethorphan|Respiratory|0|2|Syrup=10mg/5ml
Acetylcysteine|Respiratory|1|9|Tablet=600mg
Levodropropizine|Respiratory|1|5|Syrup=30mg/5ml
Pantoprazole|Gastrointestinal|1|4|Tablet=20mg,40mg;Injection=40mg
Omeprazole|Gastrointestinal|1|2.5|Capsule=10mg,20mg,40mg
Esomeprazole|Gastrointestinal|1|6|Tablet=20mg,40mg
Rabeprazole|Gastrointestinal|1|5|Tablet=10mg,20mg
Lansoprazole|Gastrointestinal|1|5|Capsule=15mg,30mg
Famotidine|Gastrointestinal|1|2|Tablet=20mg,40mg
Domperidone|Gastrointestinal|1|2|Tablet=10mg;Syrup=1mg/ml
Ondansetron|Gastrointestinal|1|3|Tablet=4mg,8mg;Syrup=2mg/5ml;Injection=2mg/ml
Metoclopramide|Gastrointestinal|1|1.5|Tablet=10mg
Itopride|Gastrointestinal|1|6|Tablet=50mg
Loperamide|Gastrointestinal|0|2|Capsule=2mg
Racecadotril|Gastrointestinal|1|10|Capsule=100mg
Lactulose|Gastrointestinal|0|4|Syrup=10g/15ml
Bisacodyl|Gastrointestinal|0|1.5|Tablet=5mg
Sucralfate|Gastrointestinal|1|4|Suspension=1g/10ml
Dicyclomine|Gastrointestinal|1|2|Tablet=10mg,20mg
Hyoscine Butylbromide|Gastrointestinal|1|4|Tablet=10mg
Simethicone|Gastrointestinal|0|1.5|Tablet=40mg
Mesalazine|Gastrointestinal|1|10|Tablet=400mg,800mg
Ursodeoxycholic Acid|Gastrointestinal|1|14|Tablet=150mg,300mg
Rifaximin|Gastrointestinal|1|25|Tablet=200mg,400mg,550mg
Oral Rehydration Salts|Gastrointestinal|0|18|Powder=WHO-ORS
)CAT",
R"CAT(
Gabapentin|Neurological|1|5|Capsule=100mg,300mg,400mg
Pregabalin|Neurological|1|8|Capsule=50mg,75mg,150mg,300mg
Levetiracetam|Neurological|1|8|Tablet=250mg,500mg,750mg,1000mg
Carbamazepine|Neurological|1|3|Tablet=100mg,200mg,400mg
Phenytoin|Neurological|1|2|Tablet=50mg,100mg;Injection=50mg/ml
Sodium Valproate|Neurological|1|4|Tablet=200mg,500mg
Lamotrigine|Neurological|1|6|Tablet=25mg,50mg,100mg,200mg
Topiramate|Neurological|1|6|Tablet=25mg,50mg,100mg
Oxcarbazepine|Neurological|1|9|Tablet=150mg,300mg,600mg
Clonazepam|Neurological|1|2.5|Tablet=0.25mg,0.5mg,1mg,2mg
Sumatriptan|Neurological|1|25|Tablet=25mg,50mg,100mg
Rizatriptan|Neurological|1|20|Tablet=5mg,10mg
Flunarizine|Neurological|1|4|Tablet=5mg,10mg
Amitriptyline|Neurological|1|2|Tablet=10mg,25mg,50mg
Donepezil|Neurological|1|8|Tablet=5mg,10mg
Memantine|Neurological|1|10|Tablet=5mg,10mg,20mg
Levodopa + Carbidopa|Neurological|1|6|Tablet=100mg/25mg,250mg/25mg
Ropinirole|Neurological|1|8|Tablet=0.25mg,0.5mg,1mg,2mg
Pramipexole|Neurological|1|9|Tablet=0.125mg,0.25mg,0.5mg,1mg
Baclofen|Neurological|1|3|Tablet=5mg,10mg,25mg
Tizanidine|Neurological|1|5|Tablet=2mg,4mg
Betahistine|Neurological|1|4|Tablet=8mg,16mg,24mg
Escitalopram|Neurological|1|5|Tablet=5mg,10mg,20mg
Sertraline|Neurological|1|6|Tablet=25mg,50mg,100mg
Fluoxetine|Neurological|1|4|Capsule=10mg,20mg,40mg
Duloxetine|Neurological|1|9|Capsule=20mg,30mg,60mg
Olanzapine|Neurological|1|4|Tablet=2.5mg,5mg,10mg
Risperidone|Neurological|1|4|Tablet=1mg,2mg,3mg,4mg
Quetiapine|Neurological|1|7|Tablet=25mg,50mg,100mg,200mg
Zolpidem|Neurological|1|8|Tablet=5mg,10mg
Fluconazole|Anti-infectives|1|8|Tablet=50mg,100mg,150mg,200mg
Itraconazole|Anti-infectives|1|18|Capsule=100mg,200mg
Acyclovir|Anti-infectives|1|6|Tablet=200mg,400mg,800mg;Cream=5%
Valacyclovir|Anti-infectives|1|25|Tablet=500mg,1000mg
Oseltamivir|Anti-infectives|1|40|Capsule=30mg,45mg,75mg
Albendazole|Anti-infectives|0|8|Tablet=400mg;Suspension=200mg/5ml
Mebendazole|Anti-infectives|0|5|Tablet=100mg,500mg
Ivermectin|Anti-infectives|1|10|Tablet=3mg,6mg,12mg
Hydroxychloroquine|Anti-infectives|1|9|Tablet=200mg,400mg
Chloroquine|Anti-infectives|1|3|Tablet=250mg
Artemether + Lumefantrine|Anti-infectives|1|30|Tablet=20mg/120mg,80mg/480mg
Primaquine|Anti-infectives|1|3|Tablet=7.5mg,15mg
Praziquantel|Anti-infectives|1|30|Tablet=600mg
Nitazoxanide|Anti-infectives|1|14|Tablet=200mg,500mg
)CAT",
R"CAT(
Levothyroxine|Hormonal and Endocrine|1|1.8|Tablet=25mcg,50mcg,75mcg,100mcg,125mcg
Carbimazole|Hormonal and Endocrine|1|2|Tablet=5mg,10mg
Prednisolone|Hormonal and Endocrine|1|2|Tablet=5mg,10mg,20mg,40mg
Methylprednisolone|Hormonal and Endocrine|1|8|Tablet=4mg,8mg,16mg
Dexamethasone|Hormonal and Endocrine|1|1|Tablet=0.5mg,4mg;Injection=4mg/ml
Norethisterone|Hormonal and Endocrine|1|6|Tablet=5mg
Medroxyprogesterone|Hormonal and Endocrine|1|10|Tablet=10mg
Progesterone|Hormonal and Endocrine|1|20|Capsule=100mg,200mg
Cabergoline|Hormonal and Endocrine|1|40|Tablet=0.25mg,0.5mg
Tamsulosin|Genitourinary|1|8|Capsule=0.2mg,0.4mg
Finasteride|Genitourinary|1|9|Tablet=1mg,5mg
Solifenacin|Genitourinary|1|12|Tablet=5mg,10mg
Oxybutynin|Genitourinary|1|3|Tablet=2.5mg,5mg
Tadalafil|Genitourinary|1|10|Tablet=5mg,10mg,20mg
Sildenafil|Genitourinary|1|10|Tablet=25mg,50mg,100mg
Methocarbamol|Musculoskeletal|1|4|Tablet=500mg,750mg
Thiocolchicoside|Musculoskeletal|1|9|Tablet=4mg,8mg
Chlorzoxazone|Musculoskeletal|1|3|Tablet=250mg,500mg
Allopurinol|Musculoskeletal|1|2|Tablet=100mg,300mg
Febuxostat|Musculoskeletal|1|8|Tablet=40mg,80mg
Colchicine|Musculoskeletal|1|4|Tablet=0.5mg
Alendronate|Musculoskeletal|1|10|Tablet=35mg,70mg
Diacerein|Musculoskeletal|1|9|Capsule=50mg
Moxifloxacin|Ophthalmic and ENT|1|9|Eye Drops=0.5%
Ciprofloxacin Eye Drops|Ophthalmic and ENT|1|5|Eye Drops=0.3%
Tobramycin|Ophthalmic and ENT|1|7|Eye Drops=0.3%
Timolol|Ophthalmic and ENT|1|6|Eye Drops=0.25%,0.5%
Latanoprost|Ophthalmic and ENT|1|18|Eye Drops=0.005%
Carboxymethylcellulose|Ophthalmic and ENT|0|5|Eye Drops=0.5%,1%
Olopatadine|Ophthalmic and ENT|1|9|Eye Drops=0.1%
Xylometazoline|Ophthalmic and ENT|0|3|Nasal Drops=0.05%,0.1%
Ofloxacin Ear Drops|Ophthalmic and ENT|1|6|Ear Drops=0.3%
)CAT"
};
